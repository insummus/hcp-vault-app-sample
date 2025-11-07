# hcp-vault-app-sample

## Vault KV 및 Auth(AppRole) 환경 설정
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
