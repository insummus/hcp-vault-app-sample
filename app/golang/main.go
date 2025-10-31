package main

import (
	"fmt"
	"log"
	"os"
	"strings"
	"sync"
	"time"

	"github.com/hashicorp/vault/api"
	"gopkg.in/ini.v1"
)

// --- Configuration Struct ---

// Config 구조체는 설정 파일의 값을 담습니다.
type Config struct {
	VaultAddr                    string
	Namespace                    string
	RoleID                       string
	SecretID                     string
	KVMountPath                  string
	KVSecretsPaths               []string
	RenewalIntervalSeconds       time.Duration
	TokenRenewalThresholdPercent float64
}

// loadConfig는 config.ini 파일에서 설정을 로드합니다.
func loadConfig(filename string) (Config, error) {
	var cfg Config

	f, err := ini.Load(filename)
	if err != nil {
		return cfg, fmt.Errorf("❌ 설정 파일 로드 실패: %w", err)
	}

	vault := f.Section("vault")
	cfg.VaultAddr = vault.Key("addr").String()
	cfg.Namespace = vault.Key("namespace").String()
	cfg.RoleID = vault.Key("role_id").String()
	cfg.SecretID = vault.Key("secret_id").String()
	cfg.KVMountPath = vault.Key("kv_path").String()

	// 콤마로 구분된 경로 파싱
	paths := strings.Split(vault.Key("kv_secrets_paths").String(), ",")
	for _, p := range paths {
		if path := strings.TrimSpace(p); path != "" {
			cfg.KVSecretsPaths = append(cfg.KVSecretsPaths, path)
		}
	}

	interval, _ := vault.Key("renewal_interval_seconds").Int()
	cfg.RenewalIntervalSeconds = time.Duration(interval) * time.Second

	threshold, _ := vault.Key("token_renewal_threshold_percent").Float64()
	cfg.TokenRenewalThresholdPercent = threshold

	log.Printf("✅ 설정 파일 로드 완료. Vault Addr: %s", cfg.VaultAddr)
	return cfg, nil
}

// --- Vault Client Struct and Methods ---

// VaultClient는 Vault와의 통신 및 상태 관리를 담당합니다.
type VaultClient struct {
	config Config
	client *api.Client

	currentTokenMetadata *api.Secret
	secretsCache         map[string]map[string]interface{}
	stateMutex           sync.RWMutex
}

// NewVaultClient는 VaultClient를 초기화합니다.
func NewVaultClient(cfg Config) (*VaultClient, error) {
	vaultConfig := api.DefaultConfig()
	vaultConfig.Address = cfg.VaultAddr

	client, err := api.NewClient(vaultConfig)
	if err != nil {
		return nil, fmt.Errorf("Vault 클라이언트 생성 실패: %w", err)
	}

	if cfg.Namespace != "" {
		client.SetNamespace(cfg.Namespace)
	}

	return &VaultClient{
		config:       cfg,
		client:       client,
		secretsCache: make(map[string]map[string]interface{}),
	}, nil
}

// authenticate는 AppRole 인증을 수행하고 토큰을 획득합니다.
func (vc *VaultClient) authenticate() error {
	log.Println("--- 🔐 Vault AppRole 인증 시작 ---")

	options := map[string]interface{}{
		"role_id":   vc.config.RoleID,
		"secret_id": vc.config.SecretID,
	}

	secret, err := vc.client.Logical().Write("auth/approle/login", options)
	if err != nil {
		return fmt.Errorf("❌ AppRole 인증 실패: %w", err)
	}

	if secret == nil || secret.Auth == nil {
		return fmt.Errorf("❌ AppRole 인증 응답에 Auth 정보가 없음")
	}

	// 상태 업데이트 (동기화 필요)
	vc.stateMutex.Lock()
	vc.client.SetToken(secret.Auth.ClientToken)
	vc.currentTokenMetadata = secret
	vc.stateMutex.Unlock()

	log.Println("✅ Vault Auth 성공! (Auth Token 획득)")
	log.Printf("   - 토큰 TTL: %s", time.Duration(secret.Auth.LeaseDuration)*time.Second)
	log.Printf("   - 토큰 갱신 가능 여부: %t", secret.Auth.Renewable)

	return nil
}

// readKvSecret은 KV v2 Secret을 조회하고 캐시에 저장합니다.
func (vc *VaultClient) readKvSecret(path string) {
	fullPath := fmt.Sprintf("%s/data/%s", vc.config.KVMountPath, path)
	log.Printf(">>> 🔎 KV Secret 요청 URL: /v1/%s", fullPath)

	secret, err := vc.client.Logical().Read(fullPath)
	if err != nil {
		log.Printf("❌ Secret 조회 실패 (%s): %v", path, err)
		return
	}

	if secret == nil || secret.Data == nil {
		log.Printf("❌ Secret 조회 실패 (데이터 없음): %s", path)
		return
	}

	// KV v2 데이터 구조: Data["data"], Data["metadata"]
	data, ok := secret.Data["data"].(map[string]interface{})
	if !ok {
		log.Printf("❌ Secret 조회 실패 (데이터 구조 오류): %s", path)
		return
	}

	metadata, _ := secret.Data["metadata"].(map[string]interface{})
	version := "N/A"
	if v, found := metadata["version"]; found {
		version = fmt.Sprintf("%v", v)
	}

	// 캐시 업데이트
	vc.stateMutex.Lock()
	vc.secretsCache[path] = data
	vc.stateMutex.Unlock()

	log.Printf("   - ✅ Secret 조회/갱신 성공: %s, Version: %s", path, version)
}

// printSecretsCache는 현재 캐시된 시크릿 내용을 출력합니다.
func (vc *VaultClient) printSecretsCache() {
	vc.stateMutex.RLock()
	defer vc.stateMutex.RUnlock()

	log.Println("\n--- 📋 현재 Secrets Cache 내용 ---")
	for path, data := range vc.secretsCache {
		log.Printf("  [%s]", path)
		for key, value := range data {
			log.Printf("    - %s: %v", key, value)
		}
	}
	log.Println("---------------------------------")
}

// checkAndRenewToken은 토큰 상태를 확인하고 필요시 갱신합니다.
func (vc *VaultClient) checkAndRenewToken() error {
	vc.stateMutex.RLock()
	tokenMeta := vc.currentTokenMetadata
	vc.stateMutex.RUnlock()

	if tokenMeta == nil || tokenMeta.Auth == nil {
		log.Println("⚠️ 토큰 메타데이터가 없습니다. 재인증 시도.")
		return vc.authenticate()
	}

	// 토큰 자체의 TTL 조회
	lookup, err := vc.client.Auth().Token().LookupSelf()
	if err != nil {
		log.Printf("❌ 토큰 상태 조회 실패. 재인증 시도: %v", err)
		return vc.authenticate()
	}

	ttlStr, ok := lookup.Data["ttl"].(string)
	if !ok {
		return fmt.Errorf("❌ 토큰 TTL을 읽을 수 없음")
	}

	// TTL 문자열을 duration으로 파싱 (예: 1h10m3s)
	ttl, err := time.ParseDuration(ttlStr)
	if err != nil {
		return fmt.Errorf("❌ 토큰 TTL 파싱 오류: %w", err)
	}

	renewable, _ := lookup.Data["renewable"].(bool)
	initialTTL := time.Duration(tokenMeta.Auth.LeaseDuration) * time.Second
	renewalThreshold := time.Duration(vc.config.TokenRenewalThresholdPercent / 100.0 * float64(initialTTL))

	log.Printf("   - 토큰 잔여 TTL: %s (갱신 임계점: %s)", ttl, renewalThreshold)

	if ttl <= 0 {
		log.Println("🛑 토큰이 만료되었습니다. 재인증 시도.")
		return vc.authenticate()
	}

	if ttl <= renewalThreshold {
		if !renewable {
			log.Println("⚠️ 토큰이 갱신 불가능합니다. 재인증 시도.")
			return vc.authenticate()
		}

		log.Printf(">>> ⚠️ 토큰 갱신 임계점 도달! 갱신 실행... (실행전 TTL: %s)", ttl)

		// 토큰 갱신
		renewedSecret, err := vc.client.Auth().Token().RenewSelf(vc.client.Token())
		if err != nil {
			log.Printf("❌ 토큰 갱신 실패. 재인증 시도: %v", err)
			return vc.authenticate() // 갱신 실패 시 재인증 시도
		}

		// 상태 업데이트
		vc.stateMutex.Lock()
		vc.currentTokenMetadata = renewedSecret
		vc.stateMutex.Unlock()

		newTTL := time.Duration(renewedSecret.Auth.LeaseDuration) * time.Second
		log.Printf(">>> ✅ 토큰 갱신 성공! 새 TTL: %s", newTTL)
	}
	return nil
}

// startScheduledTasks는 KV Secret 갱신 및 토큰 모니터링 스케줄러를 시작합니다.
func (vc *VaultClient) startScheduledTasks() {
	interval := vc.config.RenewalIntervalSeconds

	log.Printf("--- ♻️ KV Secrets 및 토큰 갱신 모니터링 스케쥴러 시작 (Interval: %s) ---", interval)

	ticker := time.NewTicker(interval)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			// 1. 토큰 갱신 모니터링 및 로깅 수행
			log.Println("\n⏳ [스케줄러] 토큰 갱신 모니터링 시작...")
			if err := vc.checkAndRenewToken(); err != nil {
				log.Printf("❌ 토큰 관리 중 치명적 오류 발생. 다음 주기 대기. (%v)", err)
				// 오류 발생 시, 다음 틱까지 기다림
				continue
			}

			// 2. KV Secret 데이터 갱신 실행
			log.Println("\n--- ♻️ KV Secrets 갱신 스케줄러 실행 ---")
			for _, path := range vc.config.KVSecretsPaths {
				vc.readKvSecret(path)
			}
			vc.printSecretsCache()
		}
	}
}

// --- Main Function ---

func main() {
	// 로깅 설정
	log.SetOutput(os.Stdout)
	log.SetFlags(log.Ldate | log.Ltime | log.Lshortfile)

	// 1. 설정 로드
	cfg, err := loadConfig("config.ini")
	if err != nil {
		log.Fatalf("❌ 애플리케이션 시작 실패: %v", err)
	}

	// 2. Vault 클라이언트 초기화
	client, err := NewVaultClient(cfg)
	if err != nil {
		log.Fatalf("❌ 애플리케이션 시작 실패: %v", err)
	}

	// 3. AppRole 인증 및 초기 Secret 로드
	if err := client.authenticate(); err != nil {
		log.Fatalf("❌ 초기 인증 실패: %v", err)
	}

	log.Println("\n--- 🔎 초기 KV Secrets 조회 시작 ---")
	for _, path := range cfg.KVSecretsPaths {
		client.readKvSecret(path)
	}
	log.Println("✅ 초기 KV Secrets 조회 완료.")
	client.printSecretsCache()

	// 4. 스케줄러 시작 (무한 루프)
	client.startScheduledTasks()
}
