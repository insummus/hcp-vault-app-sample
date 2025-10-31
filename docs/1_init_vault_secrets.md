
# 환경 변수 선언

```bash
export VAULT_NAMESPACE="poc-main"
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

***

# CLI 명령어 (환경 변수 적용)

```bash
vault namespace create "${VAULT_NAMESPACE}"

export VAULT_NAMESPACE="${VAULT_NAMESPACE}"

vault secrets enable -namespace="${VAULT_NAMESPACE}" -path="${KV_PATH}" kv-v2

vault kv put -namespace="${VAULT_NAMESPACE}" "${KV_PATH}/${KV_SECRET_PATH}" username="${KV_USERNAME}" password="${KV_PASSWORD}" conn_url="${KV_CONN_URL}"

vault policy write -namespace="${VAULT_NAMESPACE}" "${VAULT_POLICYNAME}" - <<EOF
path "${KV_PATH}/data/*" {
  capabilities = ["read", "list"]
}
path "auth/token/renew-self" {
  capabilities = ["update"]
}
EOF

vault auth enable -namespace="${VAULT_NAMESPACE}" approle

vault write -namespace="${VAULT_NAMESPACE}" auth/approle/role/"${VAULT_APPROLE_ROLENAME}" \
  token_policies="${VAULT_POLICYNAME}" \
  token_ttl="${VAULT_APPROLE_TOKEN_TTL}" \
  token_max_ttl="${VAULT_APPROLE_TOKEN_MAX_TTL}" \
  secret_id_ttl="${VAULT_APPROLE_SECRET_ID_TTL}" \
  secret_id_num_uses="${VAULT_APPROLE_SECRET_ID_NUM_USES}"

vault read -namespace="${VAULT_NAMESPACE}" auth/approle/role/"${VAULT_APPROLE_ROLENAME}"/role-id
vault write -namespace="${VAULT_NAMESPACE}" -f auth/approle/role/"${VAULT_APPROLE_ROLENAME}"/secret-id

```
