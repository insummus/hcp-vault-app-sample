#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <jansson.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h> 

// Vault API 버전 상수
#define VAULT_API_VERSION "v1"

// --- 상수 및 전역 설정 ---
#define CONFIG_FILE "config.ini"
#define MAX_URL_SIZE 256
#define RESPONSE_BUFFER_SIZE 4096 
#define TOKEN_HEADER_BUF_SIZE 256 

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

// --- 헬퍼 함수 선언 및 구현 ---

// cURL 응답 데이터를 저장하기 위한 콜백 함수
size_t write_callback(void *contents, size_t size, size_t nmemb, char *userp) {
    size_t realsize = size * nmemb;
    strncat(userp, (const char*)contents, realsize);
    return realsize;
}

// 문자열 앞뒤 공백을 제거하는 헬퍼 함수
void trim_whitespace(char *str) {
    char *end;
    char *start = str;
    
    // 앞쪽 공백 제거
    while (*str && isspace((unsigned char)*str)) str++;
    if (*str == 0) return; 

    // 뒷쪽 공백 제거
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    
    // 결과를 원본 버퍼 시작 위치로 이동
    if (str > start) {
        memmove(start, str, strlen(str) + 1);
    }
}

// ---------------------------------------------------
// 설정 파일(config.ini)을 읽어 VaultConfig 구조체에 파싱하는 함수
// ---------------------------------------------------
int parse_config(VaultConfig *config) {
    FILE *file = fopen(CONFIG_FILE, "r");
    if (!file) {
        fprintf(stderr, "❌ [Config] 설정 파일을 열 수 없습니다: %s\n", CONFIG_FILE);
        return -1;
    }

    char line[256];
    char key[128];
    char value[128];
    int in_vault_section = 0;
    
    memset(config, 0, sizeof(VaultConfig));
    config->renewal_threshold_ratio = 0.2;
    config->secret_interval_seconds = 10;
    config->token_ttl_seconds_assumed = 120;

    while (fgets(line, sizeof(line), file)) {
        char *comment_pos = strchr(line, '#');
        if (comment_pos) *comment_pos = '\0';
        
        char temp_line[256];
        strncpy(temp_line, line, sizeof(temp_line) - 1);
        temp_line[sizeof(temp_line) - 1] = '\0';
        trim_whitespace(temp_line);
        
        if (strlen(temp_line) == 0) continue;

        if (sscanf(temp_line, "[%[^]]]", key) == 1) {
            in_vault_section = (strcmp(key, "VAULT") == 0);
        } 
        else if (in_vault_section) {
            char *eq_pos = strchr(temp_line, '=');
            if (eq_pos) {
                *eq_pos = '\0';
                strncpy(key, temp_line, sizeof(key) - 1);
                key[sizeof(key) - 1] = '\0';
                trim_whitespace(key);
                
                strncpy(value, eq_pos + 1, sizeof(value) - 1);
                value[sizeof(value) - 1] = '\0';
                trim_whitespace(value);

                if (strcmp(key, "ADDR") == 0) strncpy(config->vault_addr, value, sizeof(config->vault_addr) - 1);
                else if (strcmp(key, "NAMESPACE") == 0) strncpy(config->vault_namespace, value, sizeof(config->vault_namespace) - 1);
                else if (strcmp(key, "ROLE_ID") == 0) strncpy(config->role_id, value, sizeof(config->role_id) - 1);
                else if (strcmp(key, "SECRET_ID") == 0) strncpy(config->secret_id, value, sizeof(config->secret_id) - 1);
                else if (strcmp(key, "KV_MOUNT_POINT") == 0) strncpy(config->kv_mount_point, value, sizeof(config->kv_mount_point) - 1);
                else if (strcmp(key, "KV_SECRET_PATH") == 0) strncpy(config->kv_secret_path, value, sizeof(config->kv_secret_path) - 1);
                else if (strcmp(key, "SECRET_INTERVAL_SECONDS") == 0) config->secret_interval_seconds = atoi(value);
                else if (strcmp(key, "RENEWAL_THRESHOLD_RATIO") == 0) config->renewal_threshold_ratio = atof(value);
                else if (strcmp(key, "TOKEN_TTL_SECONDS_ASSUMED") == 0) config->token_ttl_seconds_assumed = atoi(value);
            }
        }
    }

    fclose(file);
    
    // 필수 설정 항목 누락 확인
    if (strlen(config->vault_addr) == 0 || strlen(config->role_id) == 0 || strlen(config->secret_id) == 0) {
        return -1;
    }

    return 0;
}

// --- Vault API 함수 선언 ---
int vault_authenticate();
int vault_lookup_token();
int vault_renew_token();
int vault_read_secret();

// --- 스레드 함수 선언 ---
void *token_renewal_thread(void *arg);
void *secret_scheduler_thread(void *arg);


// ----------------------------------------------------------------
// 🔑 인증 및 API 호출 구현
// ----------------------------------------------------------------

// AppRole 인증 (POST /v1/<namespace>/auth/approle/login)
int vault_authenticate() {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    char url[MAX_URL_SIZE];
    snprintf(url, MAX_URL_SIZE, "%s/%s/%s/auth/approle/login", 
             g_config.vault_addr, VAULT_API_VERSION, g_config.vault_namespace);

    char response[RESPONSE_BUFFER_SIZE] = {0};
    long http_code = 0;
    int success = -1;

    json_t *payload = json_object();
    json_object_set_new(payload, "role_id", json_string(g_config.role_id));
    json_object_set_new(payload, "secret_id", json_string(g_config.secret_id));
    char *json_payload = json_dumps(payload, JSON_COMPACT);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        if (http_code == 200) {
            json_t *root = json_loads(response, 0, NULL);
            if (root) {
                json_t *auth = json_object_get(root, "auth");
                if (auth) {
                    pthread_mutex_lock(&g_state.lock);
                    strncpy(g_state.token, json_string_value(json_object_get(auth, "client_token")), sizeof(g_state.token) - 1);
                    g_state.current_ttl = json_integer_value(json_object_get(auth, "lease_duration"));
                    g_state.renewable = json_true() == json_object_get(auth, "renewable");
                    pthread_mutex_unlock(&g_state.lock);
                    success = 0; // 성공
                    printf("✅ AppRole 인증 성공! TTL: %ld초\n", g_state.current_ttl);
                }
                json_decref(root);
            }
        } else {
            fprintf(stderr, "❌ AppRole 인증 실패: HTTP %ld. 응답: %s\n", http_code, response);
        }
    } else {
        fprintf(stderr, "❌ cURL 오류: %s\n", curl_easy_strerror(res));
    }

    free(json_payload);
    json_decref(payload);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return success;
}

// 토큰 상태 조회 (GET /v1/auth/token/lookup-self)
int vault_lookup_token() {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    char url[MAX_URL_SIZE];
    snprintf(url, MAX_URL_SIZE, "%s/%s/auth/token/lookup-self", g_config.vault_addr, VAULT_API_VERSION);
    
    char response[RESPONSE_BUFFER_SIZE] = {0};
    long http_code = 0;
    int success = -1;

    char token_header[TOKEN_HEADER_BUF_SIZE];
    snprintf(token_header, TOKEN_HEADER_BUF_SIZE, "X-Vault-Token: %s", g_state.token);
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, token_header);

    if (strlen(g_config.vault_namespace) > 0) {
        char namespace_header[64];
        snprintf(namespace_header, 64, "X-Vault-Namespace: %s", g_config.vault_namespace);
        headers = curl_slist_append(headers, namespace_header);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        if (http_code == 200) {
            json_t *root = json_loads(response, 0, NULL);
            if (root) {
                json_t *data = json_object_get(root, "data");
                if (data) {
                    pthread_mutex_lock(&g_state.lock);
                    g_state.current_ttl = json_integer_value(json_object_get(data, "ttl"));
                    g_state.renewable = json_true() == json_object_get(data, "renewable");
                    pthread_mutex_unlock(&g_state.lock);
                    success = 0;
                }
                json_decref(root);
            }
        } else {
            // 토큰이 유효하지 않으면 403을 반환할 수 있음
            fprintf(stderr, "❌ 토큰 조회 실패: HTTP %ld\n", http_code);
        }
    } else {
        fprintf(stderr, "❌ cURL 오류: %s\n", curl_easy_strerror(res));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return success;
}

// 토큰 갱신 (POST /v1/auth/token/renew-self)
int vault_renew_token() {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    char url[MAX_URL_SIZE];
    snprintf(url, MAX_URL_SIZE, "%s/%s/auth/token/renew-self", g_config.vault_addr, VAULT_API_VERSION);

    char response[RESPONSE_BUFFER_SIZE] = {0};
    long http_code = 0;
    int success = -1;

    char token_header[TOKEN_HEADER_BUF_SIZE];
    snprintf(token_header, TOKEN_HEADER_BUF_SIZE, "X-Vault-Token: %s", g_state.token);
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, token_header);

    if (strlen(g_config.vault_namespace) > 0) {
        char namespace_header[64];
        snprintf(namespace_header, 64, "X-Vault-Namespace: %s", g_config.vault_namespace);
        headers = curl_slist_append(headers, namespace_header);
    }
    
    // POST 요청이지만 payload는 {}로 빈 JSON을 사용
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{}");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        if (http_code == 200) {
            json_t *root = json_loads(response, 0, NULL);
            if (root) {
                json_t *auth = json_object_get(root, "auth");
                if (auth) {
                    pthread_mutex_lock(&g_state.lock);
                    g_state.current_ttl = json_integer_value(json_object_get(auth, "lease_duration"));
                    pthread_mutex_unlock(&g_state.lock);
                    success = 0;
                }
                json_decref(root);
            }
        } else {
            fprintf(stderr, "❌ 토큰 갱신 실패: HTTP %ld. 응답: %s\n", http_code, response);
        }
    } else {
        fprintf(stderr, "❌ cURL 오류: %s\n", curl_easy_strerror(res));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return success;
}

// KV Secret 조회 (GET /v1/<mount_point>/data/<path>)
int vault_read_secret() {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    char url[MAX_URL_SIZE];
    snprintf(url, MAX_URL_SIZE, "%s/%s/%s/data/%s", 
             g_config.vault_addr, VAULT_API_VERSION, g_config.kv_mount_point, g_config.kv_secret_path);

    printf(">>> 🔎 KV Secret 요청 URL: %s\n", url);
    
    char response[RESPONSE_BUFFER_SIZE] = {0};
    long http_code = 0;
    int success = -1;

    char token_header[TOKEN_HEADER_BUF_SIZE];
    snprintf(token_header, TOKEN_HEADER_BUF_SIZE, "X-Vault-Token: %s", g_state.token);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, token_header);

    if (strlen(g_config.vault_namespace) > 0) {
        char namespace_header[64];
        snprintf(namespace_header, 64, "X-Vault-Namespace: %s", g_config.vault_namespace);
        headers = curl_slist_append(headers, namespace_header);
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        if (http_code == 200) {
            json_t *root = json_loads(response, 0, NULL);
            if (root) {
                json_t *data_wrapper = json_object_get(root, "data");
                if (data_wrapper) {
                    json_t *secret_data = json_object_get(data_wrapper, "data");
                    json_t *metadata = json_object_get(data_wrapper, "metadata");
                    
                    if (secret_data) {
                        const char *key;
                        json_t *value;
                        
                        // 버전은 정수(Integer)로 전송되므로, long으로 읽어와 문자열로 변환
                        char version_str[16] = {0}; 
                        if (metadata) {
                            json_t *version_node = json_object_get(metadata, "version");
                            if (version_node && json_is_integer(version_node)) {
                                long version_val = json_integer_value(version_node);
                                snprintf(version_str, sizeof(version_str), "%ld", version_val);
                            }
                        }

                        printf("✅ KV Secret 데이터 조회 성공: %s/%s", g_config.kv_mount_point, g_config.kv_secret_path);
                        
                        // 버전 정보가 유효한 경우에만 (Version: %s) 출력
                        if (strlen(version_str) > 0) {
                            printf(" (Version: %s)", version_str);
                        }
                        printf("\n");
                        printf("    - Data:\n");

                        json_object_foreach(secret_data, key, value) {
                            printf("      - %s: %s\n", key, json_string_value(value) ? json_string_value(value) : json_dumps(value, JSON_COMPACT));
                        }
                        success = 0;
                    }
                }
                json_decref(root);
            }
        } else {
            fprintf(stderr, "❌ Secret 조회 실패: HTTP %ld. 응답: %s\n", http_code, response);
        }
    } else {
        fprintf(stderr, "❌ cURL 오류: %s\n", curl_easy_strerror(res));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return success;
}


// ----------------------------------------------------------------
// 🧵 스케줄링 스레드 구현
// ----------------------------------------------------------------

void *token_renewal_thread(void *arg) {
    long threshold_ttl = (long)(g_config.token_ttl_seconds_assumed * g_config.renewal_threshold_ratio);
    
    while (1) {
        printf("\n⏳ [Token Manager] 토큰 갱신 체크 시작...\n");
        
        if (vault_lookup_token() != 0) {
            fprintf(stderr, "❌ [Token Manager] 토큰 상태 조회 실패. 재인증 시도.\n");
            vault_authenticate(); 
            sleep(5);
            continue;
        }

        pthread_mutex_lock(&g_state.lock);
        long current_ttl = g_state.current_ttl;
        int renewable = g_state.renewable;
        pthread_mutex_unlock(&g_state.lock);

        printf("    ➡️ Auth Token 잔여 TTL: %ld초 (임계값: %ld초)\n", current_ttl, threshold_ttl);

        if (current_ttl <= 0 || !renewable) {
            fprintf(stderr, "🛑 [Token Manager] 토큰 만료 또는 갱신 불가. 재인증 시도.\n");
            vault_authenticate();
        } 
        else if (current_ttl <= threshold_ttl) {
            printf("🚨 TTL (%ld초)이 임계값 이하입니다. **토큰 갱신(RENEW) 시도**...\n", current_ttl);
            if (vault_renew_token() != 0) {
                fprintf(stderr, "❌ [Token Manager] 토큰 갱신 실패. 재인증 시도.\n");
                vault_authenticate(); 
            } else {
                 printf("✅ TTL 갱신 성공.\n");
            }
        } 
        else {
            printf("✅ TTL (%ld초) > 임계값. 갱신 불필요.\n", current_ttl);
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
            printf("\n--- ♻️ Secret 조회 스케줄러 실행 ---\n");
            if (vault_read_secret() != 0) {
                fprintf(stderr, "❌ [Secret Scheduler] Secret 조회 실패.\n");
            }
        } else {
            fprintf(stderr, "🛑 [Secret Scheduler] Vault에 인증되지 않았습니다. 조회 불가.\n");
        }
        
        sleep(g_config.secret_interval_seconds); 
    }
    return NULL;
}

// --- 메인 함수 ---
int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // 1. 설정 파일 로드
    if (parse_config(&g_config) != 0) {
        fprintf(stderr, "❌ [Main] 설정 파일 로드 실패. config.ini를 확인하세요.\n");
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
        pthread_mutex_destroy(&g_state.lock);
        return 1;
    }

    // 3. 스케줄링 스레드 생성
    pthread_t renew_tid, secret_tid;

    if (pthread_create(&renew_tid, NULL, token_renewal_thread, NULL) != 0 ||
        pthread_create(&secret_tid, NULL, secret_scheduler_thread, NULL) != 0) {
        fprintf(stderr, "❌ [Main] 스레드 생성 실패.\n");
        pthread_mutex_destroy(&g_state.lock);
        return 1;
    }
    
    // 4. 메인 스레드 무한 대기
    printf("\n⏰ 스케줄러 설정 완료.\n");
    printf("   - KV Secret 조회/갱신: %d초마다\n", g_config.secret_interval_seconds);
    printf("   - 토큰 갱신 체크: 5초마다\n");
    printf("\n🚀 메인 스케줄링 루프 시작. Ctrl+C로 종료하세요.\n");

    while (1) {
        sleep(1);
    }
    
    pthread_mutex_destroy(&g_state.lock);
    curl_global_cleanup();
    return 0;
}