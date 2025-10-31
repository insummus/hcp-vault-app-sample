// 파일명: src/vault_client.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <jansson.h>
#include <pthread.h>
#include <time.h>

// Vault API 버전 상수 (C언어는 헤더 파일 없이 상수를 여기에 정의합니다)
#define VAULT_API_VERSION "v1"

// --- 상수 및 전역 설정 ---
#define CONFIG_FILE "config.ini"

// Vault 설정 구조체
typedef struct {
    char vault_addr[128];
    char vault_namespace[32];
    char role_id[64];
    char secret_id[64];
    char kv_mount_point[32];
    char kv_secret_path[64];
    float renewal_threshold_ratio;
    int secret_interval_seconds;
    int token_ttl_seconds_assumed;
} VaultConfig;

// Vault 상태 구조체
typedef struct {
    char token[128];
    long current_ttl;
    int renewable;
    pthread_mutex_t lock;
} VaultState;

VaultConfig g_config;
VaultState g_state;

// --- 헬퍼 함수 선언 ---
// ... (WriteCallback, parse_config 등의 실제 구현은 생략됨. 개념 유지.)
size_t write_callback(void *contents, size_t size, size_t nmemb, char *userp) {
    size_t realsize = size * nmemb;
    strcat(userp, (char*)contents);
    return realsize;
}

// ---------------------------------------------------
// [PLACEHOLDER] 설정 파일을 읽는 함수 (간단 구현)
// ---------------------------------------------------
int parse_config(VaultConfig *config) {
    // 💡 config.ini 파싱 로직은 C에서 복잡하므로, 여기서는 플레이스홀더 값을 사용합니다.
    // 실제 사용 시 INI 파싱 라이브러리(libconfig 등)를 통합해야 합니다.
    
    // **(Placeholder Values for Configuration)**
    strcpy(config->vault_addr, "http://127.0.0.1:8200");
    strcpy(config->vault_namespace, "nxk-poc");
    strcpy(config->role_id, "eb4eb3c8-5eaf-e2da-02f2-16a9b1828023"); // 실제 Role ID로 교체
    strcpy(config->secret_id, "1f17f4c2-b6da-0a7d-8472-2149d607aa64"); // 실제 Secret ID로 교체
    strcpy(config->kv_mount_point, "nxk-kv");
    strcpy(config->kv_secret_path, "application");
    config->renewal_threshold_ratio = 0.2;
    config->secret_interval_seconds = 10;
    config->token_ttl_seconds_assumed = 120;
    
    return 0; // 0 for success
}


// --- Vault API 함수 선언 (개념 유지) ---
int vault_authenticate();
int vault_lookup_token();
int vault_renew_token();
int vault_read_secret();

// --- 스레드 함수 선언 ---
void *token_renewal_thread(void *arg);
void *secret_scheduler_thread(void *arg);

// --- 메인 함수 ---
int main() {
    // 1. 설정 파일 로드
    if (parse_config(&g_config) != 0) {
        fprintf(stderr, "❌ [Main] 설정 파일 로드 실패. 종료합니다.\n");
        return 1;
    }
    
    printf("--- Vault 클라이언트 초기화 ---\n");
    printf("URL: %s (Namespace: %s)\n", g_config.vault_addr, g_config.vault_namespace);

    // 락 초기화
    if (pthread_mutex_init(&g_state.lock, NULL) != 0) {
        fprintf(stderr, "❌ [Main] Mutex 초기화 실패.\n");
        return 1;
    }

    // 2. 초기 인증
    if (vault_authenticate() != 0) {
        fprintf(stderr, "❌ [Main] 초기 인증 실패. 종료합니다.\n");
        return 1;
    }

    // 3. 스케줄링 스레드 생성
    pthread_t renew_tid, secret_tid;

    if (pthread_create(&renew_tid, NULL, token_renewal_thread, NULL) != 0 ||
        pthread_create(&secret_tid, NULL, secret_scheduler_thread, NULL) != 0) {
        fprintf(stderr, "❌ [Main] 스레드 생성 실패.\n");
        return 1;
    }
    
    // 4. 메인 스레드 무한 대기
    printf("\n⏰ 스케줄러 설정 완료.\n");
    printf("   - KV Secret 조회/갱신: %d초마다\n", g_config.secret_interval_seconds);
    printf("   - 토큰 갱신 체크: 5초마다\n");
    printf("\n🚀 메인 스케줄링 루프 시작. Ctrl+C로 종료하세요.\n");

    while (1) {
        sleep(1); // 1초 대기하며 프로세스 유지
    }
    
    // 이 코드는 Ctrl+C로 종료되므로 실행되지 않습니다.
    pthread_mutex_destroy(&g_state.lock);
    return 0;
}


// ----------------------------------------------------------------
// 🔑 인증 및 API 호출 구현 (Placeholder 유지)
// ----------------------------------------------------------------

int vault_authenticate() {
    // 💡 Libcurl을 이용한 AppRole API 호출 및 JSON 파싱 로직 구현 필요
    // ... (실제 CURL 및 JSON 로직) ...

    // **(Placeholder Logic)**
    if (strlen(g_config.role_id) < 10) return 1;
    pthread_mutex_lock(&g_state.lock);
    strcpy(g_state.token, "hvs.dummy_token_for_c");
    g_state.current_ttl = g_config.token_ttl_seconds_assumed;
    g_state.renewable = 1;
    pthread_mutex_unlock(&g_state.lock);

    printf("✅ AppRole 인증 성공!\n");
    // [vault_lookup_token() 로직의 일부를 포함하여 토큰 ID와 TTL 출력]
    return 0;
}

int vault_lookup_token() {
    // 💡 Libcurl을 이용한 lookup-self API 호출 및 JSON 파싱 로직 구현 필요

    // **(Placeholder Logic)**
    pthread_mutex_lock(&g_state.lock);
    if (g_state.current_ttl > 0) {
        g_state.current_ttl -= 5; 
        if (g_state.current_ttl < 0) g_state.current_ttl = 0;
    }
    printf("    ➡️ Auth Token 이름 (ID): %s...\n", g_state.token);
    printf("    ➡️ Auth Token 잔여 TTL: %ld초\n", g_state.current_ttl);
    int status = g_state.current_ttl > 0;
    pthread_mutex_unlock(&g_state.lock);
    return status;
}

int vault_renew_token() {
    // 💡 Libcurl을 이용한 renew-self API 호출 로직 구현 필요

    // **(Placeholder Logic)**
    pthread_mutex_lock(&g_state.lock);
    g_state.current_ttl = g_config.token_ttl_seconds_assumed;
    pthread_mutex_unlock(&g_state.lock);
    printf("✅ 토큰 갱신 성공!\n");
    return 0;
}

int vault_read_secret() {
    // 💡 Libcurl을 이용한 KV v2 read API 호출 및 JSON 파싱 로직 구현 필요

    // **(Placeholder Logic)**
    printf("✅ KV Secret 데이터 조회 성공:\n");
    printf("    - Secret Path: %s/%s\n", g_config.kv_mount_point, g_config.kv_secret_path);
    printf("    - Version: 1 (Placeholder)\n");
    printf("    - Data:\n");
    printf("      - password: app_secret_123\n");
    printf("      - username: app_user\n");
    return 0;
}


// ----------------------------------------------------------------
// 🧵 스케줄링 스레드 구현
// ----------------------------------------------------------------

void *token_renewal_thread(void *arg) {
    long threshold_ttl = (long)(g_config.token_ttl_seconds_assumed * g_config.renewal_threshold_ratio);
    
    while (1) {
        printf("\n⏳ [Token Manager] 토큰 갱신 체크 시작...\n");
        
        vault_lookup_token();

        pthread_mutex_lock(&g_state.lock);
        long current_ttl = g_state.current_ttl;
        int renewable = g_state.renewable;
        pthread_mutex_unlock(&g_state.lock);

        if (current_ttl <= 0 || !renewable) {
            fprintf(stderr, "🛑 [Token Manager] 토큰 만료 또는 갱신 불가. 재인증이 필요합니다.\n");
            vault_authenticate(); // 재인증 시도
        } else if (current_ttl <= threshold_ttl) {
            printf("🚨 TTL (%ld초)이 임계값 (%ld초) 이하입니다. **토큰 갱신(RENEW) 시도**...\n", current_ttl, threshold_ttl);
            vault_renew_token();
        } else {
            printf("✅ TTL (%ld초) > 임계값 (%ld초). 갱신 불필요.\n", current_ttl, threshold_ttl);
        }

        sleep(5);
    }
    return NULL;
}

void *secret_scheduler_thread(void *arg) {
    while (1) {
        pthread_mutex_lock(&g_state.lock);
        int authenticated = (strlen(g_state.token) > 0);
        pthread_mutex_unlock(&g_state.lock);

        if (authenticated) {
            vault_read_secret();
        } else {
            fprintf(stderr, "🛑 [Secret Scheduler] Vault에 인증되지 않았습니다. 조회 불가.\n");
        }
        
        sleep(g_config.secret_interval_seconds);
    }
    return NULL;
}