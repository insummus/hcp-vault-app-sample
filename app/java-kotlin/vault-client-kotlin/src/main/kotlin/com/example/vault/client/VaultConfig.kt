package com.example.vault.client

import com.typesafe.config.ConfigFactory
import java.time.Duration

// 로깅 라이브러리 대신 표준 출력 사용
private fun logInfo(message: String) = println("[INFO] VaultConfig: $message")
private fun logError(message: String) = System.err.println("[ERROR] VaultConfig: $message")


// TypeSafe Config를 사용하여 설정 로드
data class VaultConfig(
    val vaultAddr: String,
    val namespace: String,
    val roleId: String,
    val secretId: String,
    val kvMountPath: String,
    val kvSecretsPaths: List<String>,
    val renewalInterval: Duration,
    val renewalThresholdRatio: Double
) {
    companion object {
        fun load(): VaultConfig {
            logInfo("⏳ 설정 파일 (application.properties) 로드 시작...")
            val config = ConfigFactory.load().getConfig("vault")

            val vaultConfig = VaultConfig(
                vaultAddr = config.getString("addr"),
                namespace = config.getString("namespace"),
                roleId = config.getString("role_id"),
                secretId = config.getString("secret_id"),
                kvMountPath = config.getString("kv_mount_path"),
                kvSecretsPaths = config.getStringList("kv_secrets_paths"),
                renewalInterval = Duration.ofSeconds(config.getLong("renewal_interval_seconds")),
                renewalThresholdRatio = config.getInt("token_renewal_threshold_percent") / 100.0
            )

            logInfo("✅ 설정 파일 로드 완료. Vault Address: ${vaultConfig.vaultAddr}")
            return vaultConfig
        }
    }
}