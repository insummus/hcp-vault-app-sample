
```bash
# 셸 변수
VAULT_VERSION=1.20.3

# 디렉토리
mkdir -p /etc/vault /opt/vault/data /etc/vault/license

# 최종 오류 수정: VAULT_URL 정의 시 백슬래시 제거 및 URL 안전하게 구성
VAULT_URL="https://releases.hashicorp.com/vault/${var.vault_version}+ent/vault_${var.vault_version}+ent_linux_amd64.zip"

curl -o /tmp/vault.zip $VAULT_URL
unzip /tmp/vault.zip -d /usr/local/bin/
rm /tmp/vault.zip
chmod +x /usr/local/bin/vault

aws s3 cp s3://$S3_BUCKET_ID/$LICENSE_FILE_KEY /etc/vault/license/vault.hclic || true

# -----------------------------------------------------------------
# Vault Configuration (Raft HA & KMS Auto-Unseal)
# -----------------------------------------------------------------
cat << EOF_HCL > /etc/vault/vault.hcl
# /etc/vault/vault.hcl

listener "tcp" {
  address     = "0.0.0.0:8200"
  tls_disable = true
}

storage "raft" {
  path    = "/opt/vault/data"
  node_id = "node1"
}

api_addr     = "http://$(hostname -I | awk '{print $1}'):8200"
cluster_addr = "http://$(hostname -I | awk '{print $1}'):8201"

ui           = true
disable_mlock = true
license_path = "/etc/vault/license/vault.hclic"
EOF_HCL
# -----------------------------------------------------------------

# systemd Service file
cat << EOF_SERVICE > /etc/systemd/system/vault.service
[Unit]
Description="HashiCorp Vault - A tool for managing secrets"
Documentation=https://www.vaultproject.io/docs/
Requires=network-online.target
After=network-online.target

[Service]
Environment="VAULT_CONFIG_FILE=/etc/vault/vault.hcl"
ExecStart=/usr/local/bin/vault server -config=/etc/vault/vault.hcl
KillMode=process
KillSignal=SIGINT
Restart=on-failure
RestartSec=5
LimitMEMLOCK=infinity
Capabilities=CAP_IPC_LOCK+ep
CapabilityBoundingSet=CAP_SYSLOG CAP_IPC_LOCK

StandardOutput=journal
StandardError=journal

User=vault
Group=vault

[Install]
WantedBy=multi-user.target
EOF_SERVICE

useradd --system --home /etc/vault --shell /bin/false vault || true
chown -R vault:vault /etc/vault /opt/vault/data /etc/vault/license
chmod 640 /etc/vault/vault.hcl
chmod 640 /etc/vault/license/vault.hclic
sudo vi /etc/vault/license/vault.hclic

systemctl daemon-reload
systemctl enable vault
systemctl start vault


# Vault Init
export VAULT_ADDR="http://127.0.0.1:8200"

vault operator init -key-shares=1 -key-threshold=1 > init.key

vault operator unseal

vault login


```