package com.example.vault.client;

import org.apache.commons.configuration.Configuration;
import org.apache.commons.configuration.PropertiesConfiguration;
import org.apache.commons.configuration.ConfigurationException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class VaultConfig {
    private static final Logger log = LoggerFactory.getLogger(VaultConfig.class);

    public final String vaultAddr;
    public final String namespace;
    public final String roleId;
    public final String secretId;
    public final String[] kvSecretsPaths;
    public final String kvMountPath; 
    public final long kvRenewalIntervalSeconds;
    public final double tokenRenewalThresholdPercent;

    public VaultConfig() throws ConfigurationException {
        log.info("⏳ 설정 파일 (config.properties) 로드 시작...");
        Configuration config = new PropertiesConfiguration("config.properties");

        this.vaultAddr = config.getString("vault.vault_addr");
        this.namespace = config.getString("vault.vault_namespace");
        this.roleId = config.getString("vault.vault_role_id");
        this.secretId = config.getString("vault.vault_secret_id");
        this.kvRenewalIntervalSeconds = config.getInt("kv_renewal_interval_seconds");
        this.tokenRenewalThresholdPercent = config.getDouble("token_renewal_threshold_percent");
        
        this.kvMountPath = config.getString("kv_mount_path", "kv"); 
        
        this.kvSecretsPaths = config.getStringArray("kv_secrets_paths");
        if (this.kvSecretsPaths.length == 0 || this.kvSecretsPaths[0].isEmpty()) {
             log.warn("config.properties에 'kv_secrets_paths'가 설정되지 않았습니다. Secret 조회가 비활성화됩니다.");
        }
        
        log.info("✅ 설정 파일 로드 완료. Vault Address: {}", vaultAddr);
    }
}