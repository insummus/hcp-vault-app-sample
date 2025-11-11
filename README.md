# hcp-vault-app-sample



## 언어 별 Vault 라이브러리 

| 언어          | 라이브러리명                                                 | 공식/비공식                       | 사용 방식/예제                                               | 지원 기능 범위                                               | 문서 링크                                                    |
| :------------ | :----------------------------------------------------------- | :-------------------------------- | :----------------------------------------------------------- | :----------------------------------------------------------- | :----------------------------------------------------------- |
| **Golng**     | **[vault/api](https://pkg.go.dev/github.com/hashicorp/vault/api) [vault-client-go](https://github.com/hashicorp/vault-client-go)** | **HashiCorp 공식**                | **go get github.com/hashicorp/vault-client-go 객체 생성 후 API 호출** | **Vault 전 영역(공식 지원, 최신 반영)**                      | **[Go 공식 API](https://pkg.go.dev/github.com/hashicorp/vault/api)** |
| **.NET (C#)** | **[HashiCorp.Vault](https://github.com/hashicorp/vault-client-dotnet) [VaultSharp](https://github.com/rajanadar/VaultSharp)** | **HashiCorp 공식(OpenAPI, Beta)** | **Nuget 패키지 배포 객체 생성 후 메서드 사용**               | **OpenAPI 기반 모든 엔드포인트, 인증 방식, KV, Transit 등**  | **[.NET 공식 문서](https://github.com/hashicorp/vault-client-dotnet)** |
| Java          | [spring-vault](https://docs.spring.io/spring-vault/docs/current/reference/html/) [vault-java-driver](https://github.com/BetterCloud/vault-java-driver) [vault-java](https://github.com/jhaals/vault-java) | 커뮤니티                          | Spring Integration 또는 객체 생성 후 API 사용                | 인증방식, KV, Transit, PKI 등 (spring-vault는 Spring 연동 특화) | [spring-vault](https://docs.spring.io/spring-vault/docs/current/reference/html/) [vault-java-driver](https://github.com/BetterCloud/vault-java-driver) |
| Java (Kotlin) | [kault](https://github.com/Hansanto/kault) [vault-kotlin](https://github.com/kunickiaj/vault-kotlin) | 커뮤니티                          | 객체 생성 후 메소드 사용                                     | 제한적(인증, KV 등)                                          | [kault](https://github.com/Hansanto/kault)                   |
| C             | 직접 HTTP API 호출, 오픈소스 소수                            | 공식 없음                         | curl/http로 직접 연동 또는 소규모 프로젝트 활용              | RestAPI 로 처리                                              | [Vault HTTP API Spec](https://developer.hashicorp.com/vault/api-docs) |
| C++           | [libvault](https://github.com/abedra/libvault)               | 커뮤니티                          | Vault::Client 등 객체 사용, cURL 기반                        | 제한적(인증, KV, Transit 등)                                 | [libvault](https://github.com/abedra/libvault)               |
| Node.js       | [node-vault](https://github.com/nodevault/node-vault) [node-vault-client](https://www.npmjs.com/package/node-vault-client) | 커뮤니티                          | npm install node-vault require 후 메소드 사용                | 제한적(인증, KV, Transit 등)                                 | [node-vault 명세](https://github.com/nodevault/node-vault)   |
| Python        | [hvac](https://pypi.org/project/hvac/)                       | 커뮤니티                          | pip install hvac import hvac 후 사용                         | 대부분 지원(인증, KV, Transit, PKI 등)                       | [hvac 공식문서](https://hvac.readthedocs.io/)                |

### 세부 내용

#### 공식/비공식 구분

- **HashiCorp 공식 지원**: Go, .NET(OpenAPI 기반)
- **커뮤니티**: Python, Java, Kotlin, C/C++, Node.js

#### 사용 방식 및 실제 예시 코드

- 대부분 Client/Session 객체 생성하여 API 호출( `client.read("secret/foo")`)

#### 지원 기능 범위

- Go, .NET 공식 클라이언트는 최신 API의 대부분(모든 주요 엔진, 인증, wrapping, ACL 등) 구현
- Java, Python(hvac), Node.js(node-vault) 등은 인증, KV, Transit, PKI 등 Vault 핵심 기능 대부분 지원
- Kotlin, C/C++ 등은 상대적 지원 범위가 제한적, RestAPI/cURL 등을 사용하여 직접 Vault 기능 사용 권장

#### 공식 문서 링크

- HTTP API 문서: https://developer.hashicorp.com/vault/api-docs



## 언어 별 샘플 코드에서 사용된 라이브러리 또는 방식

| **언어**          | **Vault 통신 방식**                | 사용한 라이브러리 또는 방식                                  |
| ----------------- | ---------------------------------- | ------------------------------------------------------------ |
| **Golang **       | **공식 Vault API 클라이언트** 사용 | `github.com/hashicorp/vault/api`                             |
| **.NET(C#)**      | **REST API 호출**                  | **`System.Net.Http.HttpClient`**, **`System.Text.Json`**     |
| **Java**          | **REST API 호출**                  | **Apache HttpClient 5** (`org.apache.hc.client5`), **Jackson** (`com.fasterxml.jackson.databind`) |
| **Java (Kotlin)** | **REST API 호출**                  | **OkHttp** (`okhttp3`), **Moshi** (`com.squareup.moshi`)     |
| **C**             | **REST API 호출**                  | **libcurl** (`curl/curl.h`), **jansson** (`jansson.h`)       |
| **C++**           | **REST API 호출**)                 | **libcurl** (`curl/curl.h`), **nlohmann/json** (`nlohmann/json.hpp`) |
| **Node.js**       | **REST API 호출**                  | **Axios** (`axios`), `node-schedule`                         |
| **Python**        | **Vault API 클라이언트** 사용      | **hvac**                                                     |



## 테스트 환경 샘플 코드 워크플로우

```mermaid
sequenceDiagram
    participant VaultServerAdmin as Vault Server(admin)
    participant BastionApp as Bastion(App)
    participant VaultServer as Vault Server

    Note over VaultServerAdmin, VaultServer: 배포 환경 설정 (Admin Setup)
    VaultServerAdmin->>VaultServer: 1. KV Secrets Engine 활성화 및 정책 생성
    VaultServerAdmin->>VaultServer: 2. AppRole 활성화 및 역할 생성 (Role ID 발급)
    VaultServerAdmin->>VaultServer: 3. KV Secret 데이터 입력 (kv_app/application)
    VaultServerAdmin->>VaultServerAdmin: 4. Role ID / Secret ID 발급 및 기록
    VaultServerAdmin->>BastionApp: 5. Role ID / Secret ID를 설정 파일에 주입

    rect rgb(240, 240, 255)
        Note over BastionApp, VaultServer: 런타임 - 초기 인증 및 조회
        BastionApp->>VaultServer: 6. POST /v1/auth/approle/login (Role ID, Secret ID)
        VaultServer-->>BastionApp: 7. Auth Token 발급 (TTL, Renewable 포함)
        BastionApp->>VaultServer: 8. GET /v1/kv_app/data/application (Auth Token 사용)
        VaultServer-->>BastionApp: 9. Secret 데이터 응답 (username, password 등)
        BastionApp->>BastionApp: 10. Secret 캐싱 및 스케줄러 시작
    end

    loop 주기적인 토큰/Secret 모니터링 (Interval: 10s)
        BastionApp->>BastionApp: 11. 현재 토큰 TTL 계산
        alt 12. TTL < 임계점 (20%) 또는 만료 상태
            BastionApp->>VaultServer: 13. POST /v1/auth/token/renew-self (Auth Token)
            alt 14. Renewal 성공
                VaultServer-->>BastionApp: 15. Renewed Token / New TTL 응답
            else 14. Renewal 실패 (만료 또는 갱신 불가)
                Note over BastionApp, VaultServer: 토큰 만료 또는 갱신 불가 -> 재인증 시도 (6단계로 이동)
                BastionApp->>VaultServer: 16. POST /v1/auth/approle/login (재인증)
            end
        else 12. TTL >= 임계점
            Note right of BastionApp: 갱신 불필요. 토큰 유효함.
        end

        BastionApp->>VaultServer: 17. GET /v1/kv_app/data/application (Secret 최신 버전 조회)
        VaultServer-->>BastionApp: 18. Secret 데이터 응답
        BastionApp->>BastionApp: 19. Secret 캐시 업데이트
    end
```





## 테스트 환경 설정 - Vault KV 및 Auth(AppRole)

### 환경 변수 선언
```bash
export VAULT_ADDR="http://127.0.0.1:8200"
export VAULT_POC_NAMESPACE="poc-main"
export KV_PATH="kv_app"
export KV_SECRET_PATH="application"
export KV_USERNAME="admin"
export KV_PASSWORD="1234"
export KV_CONN_URL="10.10.10.1"

export VAULT_POLICYNAME="kv-app-policy"
export VAULT_APPROLE_ROLENAME="kv-app-role"
export VAULT_APPROLE_TOKEN_TTL="2m"
export VAULT_APPROLE_TOKEN_MAX_TTL="1h"
export VAULT_APPROLE_SECRET_ID_TTL="1h"
export VAULT_APPROLE_SECRET_ID_NUM_USES="0"

```

### CLI 명령어 (환경 변수 적용)
```bash
# 기본 설정
vault namespace create "${VAULT_POC_NAMESPACE}"
export VAULT_NAMESPACE="${VAULT_POC_NAMESPACE}"

# 시크릿 엔진 생성 및 샘플 데이터 구성
vault secrets enable -namespace="${VAULT_NAMESPACE}" -path="${KV_PATH}" kv-v2

vault kv put -namespace="${VAULT_NAMESPACE}" "${KV_PATH}/${KV_SECRET_PATH}" username="${KV_USERNAME}" password="${KV_PASSWORD}" conn_url="${KV_CONN_URL}"

# 정책 생성
vault policy write -namespace="${VAULT_NAMESPACE}" "${VAULT_POLICYNAME}" - <<EOF
path "${KV_PATH}/data/*" {
  capabilities = ["read", "list"]
}
path "auth/token/renew-self" {
  capabilities = ["update"]
}
EOF

# 인증 생성
vault auth enable -namespace="${VAULT_NAMESPACE}" approle

vault write -namespace="${VAULT_NAMESPACE}" auth/approle/role/"${VAULT_APPROLE_ROLENAME}" \
  token_policies="${VAULT_POLICYNAME}" \
  token_ttl="${VAULT_APPROLE_TOKEN_TTL}" \
  token_max_ttl="${VAULT_APPROLE_TOKEN_MAX_TTL}" \
  secret_id_ttl="${VAULT_APPROLE_SECRET_ID_TTL}" \
  secret_id_num_uses="${VAULT_APPROLE_SECRET_ID_NUM_USES}"


# approle 발급
vault read -namespace="${VAULT_NAMESPACE}" auth/approle/role/"${VAULT_APPROLE_ROLENAME}"/role-id >> approle_id.txt
vault write -namespace="${VAULT_NAMESPACE}" -f auth/approle/role/"${VAULT_APPROLE_ROLENAME}"/secret-id >> approle_id.txt

# approle login(확인목적)
vault write -namespace="${VAULT_NAMESPACE}" auth/approle/login role_id=xxxx secret_id=xxx
```
