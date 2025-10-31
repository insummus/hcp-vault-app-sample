package com.example.vault.client;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.apache.hc.client5.http.classic.methods.HttpGet;
import org.apache.hc.client5.http.classic.methods.HttpPost;
import org.apache.hc.client5.http.impl.classic.CloseableHttpClient;
import org.apache.hc.client5.http.impl.classic.CloseableHttpResponse;
import org.apache.hc.client5.http.impl.classic.HttpClients;
import org.apache.hc.core5.http.ContentType;
import org.apache.hc.core5.http.io.entity.EntityUtils;
import org.apache.hc.core5.http.io.entity.StringEntity;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.time.Instant;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

public class VaultHttpClient {

    private static final Logger log = LoggerFactory.getLogger(VaultHttpClient.class);
    private final ObjectMapper mapper = new ObjectMapper();
    private final VaultConfig config;

    // Vault 상태 변수
    private volatile String currentToken = "";
    private volatile long leaseDurationSeconds = 0;
    private volatile long authTimeEpochSeconds = 0;
    private volatile boolean isRenewable = false;
    
    // 시크릿 저장소 (스레드 안전한 캐시)
    private final Map<String, Map<String, String>> secretsCache = new ConcurrentHashMap<>();

    public static void main(String[] args) {
        try {
            VaultConfig config = new VaultConfig();
            VaultHttpClient client = new VaultHttpClient(config);

            // 1. AppRole 인증 및 초기 Secret 로드
            client.authenticateAndLoadSecrets();

            // 2. 스케줄러 시작
            client.startScheduledTasks();

            // 애플리케이션 유지를 위해 메인 스레드 대기
            Thread.currentThread().join();

        } catch (org.apache.commons.configuration.ConfigurationException e) {
            log.error("❌ 설정 파일 로드 실패: config.properties를 찾을 수 없습니다.", e);
        } catch (Exception e) {
            log.error("❌ 애플리케이션 치명적 오류 발생", e);
        }
    }

    public VaultHttpClient(VaultConfig config) {
        this.config = config;
    }
    
    /** 현재 토큰의 잔여 TTL을 계산하여 반환합니다. */
    private long getRemainingTtl() {
        long currentTimeEpoch = Instant.now().getEpochSecond();
        long elapsed = currentTimeEpoch - authTimeEpochSeconds;
        return leaseDurationSeconds - elapsed;
    }


    // =================================
    // 1. 인증: AppRole 로그인 (REST API: POST /v1/auth/approle/login)
    // =================================
    public void authenticateAndLoadSecrets() throws Exception {
        log.info("--- 🔐 Vault AppRole 인증 시작 ---");

        String url = config.vaultAddr + "/v1/auth/approle/login";
        
        Map<String, String> payload = new HashMap<>();
        payload.put("role_id", config.roleId);
        payload.put("secret_id", config.secretId);
        String jsonPayload = mapper.writeValueAsString(payload);

        try (CloseableHttpClient httpClient = HttpClients.createDefault();
             CloseableHttpResponse response = executePost(httpClient, url, jsonPayload, null)) {

            String responseBody = EntityUtils.toString(response.getEntity());
            if (response.getCode() != 200) {
                log.error("❌ AppRole 인증 실패. HTTP Code: {}, Body: {}", response.getCode(), responseBody);
                throw new IOException("AppRole 인증 실패");
            }

            JsonNode root = mapper.readTree(responseBody);
            JsonNode auth = root.get("auth");

            currentToken = auth.get("client_token").asText();
            leaseDurationSeconds = auth.get("lease_duration").asLong();
            isRenewable = auth.get("renewable").asBoolean();
            authTimeEpochSeconds = Instant.now().getEpochSecond();

            log.info("✅ Vault Auth 성공! (Auth Token 획득)");
            log.info("   - 토큰 스트링 (일부): {}...", currentToken.substring(0, 10));
            log.info("   - 토큰 lease time (TTL): {} 초", leaseDurationSeconds);
            log.info("   - 토큰 만료 기간: {} (Epoch Seconds)", authTimeEpochSeconds + leaseDurationSeconds);
            log.info("   - 토큰 갱신 가능 여부: {}", isRenewable);

            // 초기 KV 데이터 조회
            log.info("\n--- 🔎 초기 KV Secrets 조회 시작 ---");
            for (String path : config.kvSecretsPaths) {
                readKvSecret(path);
            }
            log.info("✅ 초기 KV Secrets 조회 완료.");
            printSecretsCache();
        }
    }

    // =================================
    // 2. 스케줄러 (토큰 갱신 및 KV Secret 갱신)
    // =================================
    public void startScheduledTasks() {
        // 토큰 갱신과 KV Secret 갱신을 하나의 스케줄러로 통합합니다.
        ScheduledExecutorService scheduler = Executors.newSingleThreadScheduledExecutor();

        // KV 데이터 갱신 및 토큰 모니터링 스케줄러 (설정된 interval로 실행)
        scheduler.scheduleAtFixedRate(() -> {
            
            // 1. 토큰 갱신 모니터링 및 로깅 수행
            // ------------------------------------------------
            long remainingTtl = getRemainingTtl();
            long renewalThreshold = (long) (leaseDurationSeconds * (config.tokenRenewalThresholdPercent / 100.0));
            log.info("   - 토큰 잔여 TTL: {} 초 (갱신 임계점: {} 초)", remainingTtl, renewalThreshold);

            // 토큰 갱신 임계점 도달 확인 및 갱신 실행
            if (remainingTtl <= renewalThreshold && remainingTtl > 0) {
                 try {
                     manualRenewToken(remainingTtl);
                 } catch (Exception e) {
                     log.error("❌ 토큰 갱신 중 예외 발생: {}", e.getMessage(), e);
                 }
            }
            // ------------------------------------------------

            // 2. KV Secret 데이터 갱신 실행
            log.info("\n--- ♻️ KV Secrets 갱신 스케줄러 실행 ---");
            for (String path : config.kvSecretsPaths) {
                try {
                    readKvSecret(path);
                } catch (Exception e) {
                    log.error("❌ KV Secret 갱신 실패 ({}): {}", path, e.getMessage());
                }
            }
            printSecretsCache();
        }, config.kvRenewalIntervalSeconds, config.kvRenewalIntervalSeconds, TimeUnit.SECONDS);
        
        // 시작 로그 출력
        log.info("--- ♻️ KV Secrets 및 토큰 갱신 모니터링 스케쥴러 시작 (Interval: {}초) ---", config.kvRenewalIntervalSeconds);
    }
    
    /** 토큰 갱신 로직 (REST API 호출) */
    private void manualRenewToken(long remainingTtl) throws Exception {
        if (!isRenewable) {
            log.error("⚠️ 토큰이 갱신 불가능합니다. 재인증이 필요합니다.");
            return;
        }

        log.warn(">>> ⚠️ 토큰 갱신 임계점 도달! 갱신 실행... (실행전 TTL: {}초)", remainingTtl);
        try (CloseableHttpClient httpClient = HttpClients.createDefault();
             CloseableHttpResponse response = executePost(httpClient, config.vaultAddr + "/v1/auth/token/renew-self", "{}", currentToken)) {
            
            String responseBody = EntityUtils.toString(response.getEntity());
            if (response.getCode() != 200) {
                log.error("❌ 토큰 갱신 실패. HTTP Code: {}, Body: {}", response.getCode(), responseBody);
                throw new IOException("토큰 갱신 REST API 실패");
            }

            JsonNode auth = mapper.readTree(responseBody).get("auth");
            
            long oldTtl = leaseDurationSeconds;
            leaseDurationSeconds = auth.get("lease_duration").asLong();
            authTimeEpochSeconds = Instant.now().getEpochSecond();
            
            log.info(">>> ✅ 토큰 갱신 성공!");
            log.info("    - 실행후 새로운 TTL: {} 초 (이전 TTL: {} 초)", leaseDurationSeconds, oldTtl);

        }
    }
    
    // =================================
    // 3. KV Secret 조회 (REST API: GET /v1/<mount_path>/data/<path>)
    // =================================
    private void readKvSecret(String secretPath) throws Exception {
        // KV 마운트 경로를 config에서 동적으로 가져와 URL 생성
        String url = config.vaultAddr + "/v1/" + config.kvMountPath + "/data/" + secretPath; 

        // 에러 진단을 위해 요청 URL을 출력
        log.error(">>> KV Secret 요청 URL: {}", url);

        try (CloseableHttpClient httpClient = HttpClients.createDefault();
             CloseableHttpResponse response = executeGet(httpClient, url, currentToken)) {
            
            String responseBody = EntityUtils.toString(response.getEntity());
            if (response.getCode() != 200) {
                log.error("   - Secret 조회 실패. HTTP Code: {}, Path: {}", response.getCode(), secretPath);
                return;
            }

            JsonNode root = mapper.readTree(responseBody);
            JsonNode dataNode = root.get("data").get("data"); 
            JsonNode metadata = root.get("data").get("metadata");

            Map<String, String> secretData = new HashMap<>();
            
            Iterator<Map.Entry<String, JsonNode>> fields = dataNode.fields();
            while (fields.hasNext()) {
                Map.Entry<String, JsonNode> entry = fields.next();
                secretData.put(entry.getKey(), entry.getValue().asText());
            }

            secretsCache.put(secretPath, secretData);
            
            String version = (metadata != null && metadata.has("version")) 
                             ? metadata.get("version").asText() : "N/A";
                             
            log.info("   - Secret 조회/갱신 성공: {}, Version: {}", secretPath, version);
        }
    }

    private void printSecretsCache() {
        log.info("\n--- 📋 현재 Secrets Cache 내용 ---");
        secretsCache.forEach((path, data) -> {
            log.info("  [{}]", path);
            data.forEach((key, value) -> log.info("    - {}: {}", key, value));
        });
        log.info("---------------------------------");
    }

    // =================================
    // HTTP Utility Functions
    // =================================
    private CloseableHttpResponse executePost(CloseableHttpClient client, String url, String jsonPayload, String token) throws IOException {
        HttpPost httpPost = new HttpPost(url);
        httpPost.setEntity(new StringEntity(jsonPayload, ContentType.APPLICATION_JSON));
        
        httpPost.setHeader("Content-Type", ContentType.APPLICATION_JSON.getMimeType());
        if (config.namespace != null && !config.namespace.isEmpty()) {
            httpPost.setHeader("X-Vault-Namespace", config.namespace);
        }
        if (token != null) {
            httpPost.setHeader("X-Vault-Token", token);
        }
        return client.execute(httpPost);
    }
    
    private CloseableHttpResponse executeGet(CloseableHttpClient client, String url, String token) throws IOException {
        HttpGet httpGet = new HttpGet(url);
        
        if (config.namespace != null && !config.namespace.isEmpty()) {
            httpGet.setHeader("X-Vault-Namespace", config.namespace);
        }
        if (token != null) {
            httpGet.setHeader("X-Vault-Token", token);
        }
        return client.execute(httpGet);
    }
}