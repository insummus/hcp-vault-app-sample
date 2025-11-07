package com.example.vault.client

import com.squareup.moshi.JsonClass
import com.squareup.moshi.Moshi
import com.squareup.moshi.kotlin.reflect.KotlinJsonAdapterFactory
// import io.github.microutils.kotlin.logging.KotlinLogging // 빌드 오류 우회를 위해 제거됨
import kotlinx.coroutines.*
import okhttp3.*
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.RequestBody.Companion.toRequestBody
import java.io.IOException
import java.time.Instant
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.TimeUnit
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException
import kotlin.math.max

// 로깅 라이브러리 대신 표준 출력 사용 (Unresolved reference 오류 우회)
private val log = object {
    fun info(message: () -> String) = println("[INFO] VaultClient: ${message()}")
    fun error(e: Throwable? = null, message: () -> String) = System.err.println("[ERROR] VaultClient: ${message()}. Stack: ${e?.message ?: ""}")
    fun warn(message: () -> String) = System.err.println("[WARN] VaultClient: ${message()}")
}


// --- JSON Data Classes (Moshi) ---
@JsonClass(generateAdapter = true)
data class AuthPayload(val role_id: String, val secret_id: String)

@JsonClass(generateAdapter = true)
data class VaultAuthData(
    val client_token: String,
    val lease_duration: Long,
    val renewable: Boolean,
)

@JsonClass(generateAdapter = true)
data class VaultResponse(
    val auth: VaultAuthData? = null,
    val data: Map<String, Any>? = null,
    val errors: List<String>? = null
)

@JsonClass(generateAdapter = true)
data class VaultKvResponse(
    val data: Map<String, Any>? = null,
)

// --- Vault Client Core ---
class VaultClient(private val config: VaultConfig) {

    private val jsonMediaType = "application/json; charset=utf-8".toMediaType()
    private val moshi = Moshi.Builder().addLast(KotlinJsonAdapterFactory()).build()
    private val httpClient = OkHttpClient.Builder()
        .callTimeout(10, TimeUnit.SECONDS)
        .build()

    // Vault 상태 변수
    @Volatile private var currentToken: String = ""
    @Volatile private var leaseDurationSeconds: Long = 0
    @Volatile private var authTimeEpochSeconds: Long = 0
    @Volatile private var isRenewable: Boolean = false

    // 시크릿 저장소 (스레드 안전한 캐시)
    private val secretsCache: ConcurrentHashMap<String, Map<String, Any>> = ConcurrentHashMap()

    /** 현재 토큰의 잔여 TTL을 계산하여 반환합니다. */
    private fun getRemainingTtl(): Long {
        val currentTimeEpoch = Instant.now().epochSecond
        val elapsed = currentTimeEpoch - authTimeEpochSeconds
        return max(0, leaseDurationSeconds - elapsed)
    }

    // =================================
    // 1. 인증: AppRole 로그인
    // =================================
    suspend fun authenticate(): Boolean {
        log.info { "--- 🔐 Vault AppRole 인증 시작 ---" }

        // 네임스페이스 정보는 헤더(X-Vault-Namespace)로 전달됩니다.
	val url = "${config.vaultAddr}/v1/${config.namespace}/auth/approle/login"

        val payload = AuthPayload(config.roleId, config.secretId)
        val jsonPayload = moshi.adapter(AuthPayload::class.java).toJson(payload)

        val request = Request.Builder()
            .url(url)
            .post(jsonPayload.toRequestBody(jsonMediaType))
            .addVaultHeaders(excludeNamespace = true) 
            .build()

        return try {
            val response = executeRequest(request)
            val responseBody = response.body!!.string() 

            if (response.code != 200) {
                log.error { "❌ AppRole 인증 실패. HTTP Code: ${response.code}, Body: $responseBody" }
                throw IOException("AppRole 인증 실패")
            }

            val vaultResponse = moshi.adapter(VaultResponse::class.java).fromJson(responseBody)
            val auth = vaultResponse?.auth ?: throw IOException("인증 응답에 Auth 데이터가 없습니다.")

            currentToken = auth.client_token
            leaseDurationSeconds = auth.lease_duration
            isRenewable = auth.renewable
            authTimeEpochSeconds = Instant.now().epochSecond

            log.info { "✅ Vault Auth 성공! (Auth Token 획득)" }
            log.info { "   - 토큰 스트링 (일부): ${currentToken.substring(0, 10)}..." }
            log.info { "   - 토큰 lease time (TTL): $leaseDurationSeconds 초" }
            log.info { "   - 토큰 갱신 가능 여부: $isRenewable" }
            true
        } catch (e: Exception) {
            log.error(e) { "❌ Vault AppRole 인증 중 예외 발생: ${e.message}" }
            false
        }
    }

    // =================================
    // 2. 토큰 갱신
    // =================================
    private suspend fun manualRenewToken(remainingTtl: Long): Boolean {
        if (!isRenewable) {
            log.error { "⚠️ 토큰이 갱신 불가능합니다. 재인증이 필요합니다." }
            return false
        }

        log.warn { ">>> ⚠️ 토큰 갱신 임계점 도달! 갱신 실행... (실행전 TTL: ${remainingTtl}초)" }

        val url = "${config.vaultAddr}/v1/auth/token/renew-self"
        val request = Request.Builder()
            .url(url)
            .post("{}".toRequestBody(jsonMediaType))
            .addVaultHeaders(currentToken)
            .build()

        return try {
            val response = executeRequest(request)
            // 널 안전성 수정
            val responseBody = response.body!!.string() 

            if (response.code != 200) {
                log.error { "❌ 토큰 갱신 실패. HTTP Code: ${response.code}, Body: $responseBody" }
                throw IOException("토큰 갱신 REST API 실패")
            }

            val vaultResponse = moshi.adapter(VaultResponse::class.java).fromJson(responseBody)
            val auth = vaultResponse?.auth ?: throw IOException("갱신 응답에 Auth 데이터가 없습니다.")

            val oldTtl = leaseDurationSeconds
            leaseDurationSeconds = auth.lease_duration
            authTimeEpochSeconds = Instant.now().epochSecond

            log.info { ">>> ✅ 토큰 갱신 성공!" }
            log.info { "    - 실행후 새로운 TTL: $leaseDurationSeconds 초 (이전 TTL: $oldTtl 초)" }
            true
        } catch (e: Exception) {
            log.error(e) { "❌ 토큰 갱신 중 예외 발생: ${e.message}" }
            false
        }
    }

    // =================================
    // 3. KV Secret 조회
    // =================================
    private suspend fun readKvSecret(secretPath: String) {
        if (currentToken.isEmpty()) {
            log.error { "🛑 Secret 조회 불가: 인증 토큰이 없습니다." }
            return
        }

        val url = "${config.vaultAddr}/v1/${config.kvMountPath}/data/${secretPath}"
        log.info { ">>> 🔎 KV Secret 요청 URL: $url" }

        val request = Request.Builder()
            .url(url)
            .get()
            .addVaultHeaders(currentToken)
            .build()

        try {
            val response = executeRequest(request)
            // 널 안전성 수정
            val responseBody = response.body!!.string() 

            if (response.code != 200) {
                log.error { "   - Secret 조회 실패. HTTP Code: ${response.code}, Path: $secretPath" }
                return
            }

            // KV v2 응답의 data.data를 추출
            val rootResponse = moshi.adapter(VaultKvResponse::class.java).fromJson(responseBody)
            val dataNode = rootResponse?.data?.get("data") as? Map<String, Any> ?: emptyMap()
            val metadataNode = rootResponse?.data?.get("metadata") as? Map<String, Any> ?: emptyMap()

            // 캐시 업데이트
            secretsCache[secretPath] = dataNode

            val version = metadataNode["version"] ?: "N/A"

            log.info { "   - Secret 조회/갱신 성공: $secretPath, Version: $version" }
        } catch (e: Exception) {
            log.error(e) { "❌ KV Secret 조회 중 예외 발생 ($secretPath): ${e.message}" }
        }
    }

    private fun printSecretsCache() {
        log.info { "\n--- 📋 현재 Secrets Cache 내용 ---" }
        secretsCache.forEach { (path, data) ->
            log.info { "  [$path]" }
            data.forEach { (key, value) ->
                log.info { "    - $key: $value" }
            }
        }
        log.info { "---------------------------------" }
    }

    // =================================
    // 4. 스케줄링 및 모니터링
    // =================================
    private fun startScheduledTasks() {
        log.info { "--- ♻️ KV Secrets 및 토큰 갱신 모니터링 스케쥴러 시작 (Interval: ${config.renewalInterval.seconds}초) ---" }

        // Coroutine을 사용하여 주기적인 작업 실행
        val scope = CoroutineScope(Dispatchers.Default)

        scope.launch {
            while (isActive) {
                delay(config.renewalInterval.toMillis())
                scheduledTask()
            }
        }
    }

    private suspend fun scheduledTask() {
        var isAuthenticated = currentToken.isNotEmpty()

        // 1. 토큰 갱신 모니터링 및 로깅 수행
        if (!isAuthenticated) {
            log.warn { "\n🛑 토큰이 없습니다. 재인증 시도..." }
            isAuthenticated = authenticate()
        }
        
        if (isAuthenticated) {
            val remainingTtl = getRemainingTtl()
            val renewalThreshold = (leaseDurationSeconds * config.renewalThresholdRatio).toLong()
            log.info { "   - 토큰 잔여 TTL: $remainingTtl 초 (갱신 임계점: $renewalThreshold 초)" }

            if (remainingTtl <= 0) {
                log.error { "🛑 토큰 만료! 재인증을 시도합니다..." }
                isAuthenticated = authenticate()
            } else if (remainingTtl <= renewalThreshold) {
                val success = manualRenewToken(remainingTtl)
                if (!success) {
                    // 갱신 실패 시 재인증 시도
                    isAuthenticated = authenticate()
                }
            } else {
                log.info { "✅ TTL > 임계값. 갱신 불필요." }
            }
        } else {
            log.error { "❌ AppRole 인증에 실패했습니다. Secret 조회 스케줄러를 건너뜁니다." }
        }
        
        // 2. KV Secret 데이터 갱신 실행
        if (isAuthenticated) {
            log.info { "\n--- ♻️ KV Secrets 갱신 스케줄러 실행 ---" }
            config.kvSecretsPaths.forEach { path ->
                readKvSecret(path)
            }
            printSecretsCache()
        }
    }

    // =================================
    // 5. 메인 실행 함수
    // =================================
    suspend fun run() {
        // 1. 초기 인증
        var success = authenticate()

        if (!success) {
            log.error { "❌ 애플리케이션 초기 인증 실패. 종료합니다." }
            return
        }

        // 2. 초기 KV Secret 로드
        log.info { "\n--- 🔎 초기 KV Secrets 조회 시작 ---" }
        config.kvSecretsPaths.forEach { path ->
            readKvSecret(path)
        }
        log.info { "✅ 초기 KV Secrets 조회 완료." }
        printSecretsCache()

        // 3. 스케줄러 시작
        startScheduledTasks()
    }

    // --- Utility Extensions ---

    /** OkHttp Call을 suspend function으로 변환 */
    private suspend fun executeRequest(request: Request): Response = suspendCancellableCoroutine { continuation ->
        // Call 객체를 변수로 저장
        val call = httpClient.newCall(request) 
        
        // 코루틴 취소 시 OkHttp 요청도 취소하도록 핸들러 추가
        continuation.invokeOnCancellation {
            call.cancel()
        }
        
        // Call 객체를 사용하여 enqueue
        call.enqueue(object : Callback { 
            override fun onFailure(call: Call, e: IOException) {
                continuation.resumeWithException(e)
            }

            override fun onResponse(call: Call, response: Response) {
                continuation.resume(response)
            }
        })
    }
    
    /** Request.Builder에 Vault 공통 헤더를 추가합니다. */
    // 네임스페이스 헤더의 포함 여부를 제어합니다. (AppRole 인증 시에만 제외)
    private fun Request.Builder.addVaultHeaders(token: String? = null, excludeNamespace: Boolean = false): Request.Builder {
        header("Content-Type", jsonMediaType.toString())
        
        // 네임스페이스 헤더 추가 로직
        if (config.namespace.isNotEmpty() && !excludeNamespace) {
            header("X-Vault-Namespace", config.namespace)
        }
        
        if (token != null && token.isNotEmpty()) {
            header("X-Vault-Token", token)
        }
        return this
    }
}

// =================================
// Kotlin 메인 함수
// =================================
fun main() = runBlocking {
    try {
        // VaultConfig.load()는 VaultConfig.kt 파일에 정의되어 있음
        val config = VaultConfig.load() 
        val client = VaultClient(config)
        
        client.run()
        
        // GlobalScope.coroutineContext.job.join() 대신 무한 대기
        delay(Long.MAX_VALUE) 
    } catch (e: Exception) {
        // 에러 로깅은 표준 출력으로 대체
        System.err.println("[FATAL ERROR] 애플리케이션 치명적 오류 발생: ${e.message}")
    }
}
