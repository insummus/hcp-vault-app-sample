package com.example.vault.client

import com.typesafe.config.ConfigFactory
import io.github.microutils.kotlin.logging.KotlinLogging
import java.time.Duration

private val log = KotlinLogging.logger {}

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
            log.info { "⏳ 설정 파일 (application.properties) 로드 시작..." }
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

            log.info { "✅ 설정 파일 로드 완료. Vault Address: ${vaultConfig.vaultAddr}" }
            return vaultConfig
        }
    }
}
