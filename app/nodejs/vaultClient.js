// 파일명: vaultClient.js

const axios = require('axios');
const config = require('config');
const schedule = require('node-schedule');

// --- 설정 로드 ---
const VAULT_CONFIG = config.get('vault');
const VAULT_ADDR = VAULT_CONFIG.addr;
const VAULT_NAMESPACE = VAULT_CONFIG.namespace;
const ROLE_ID = VAULT_CONFIG.role_id;
const SECRET_ID = VAULT_CONFIG.secret_id;
const KV_MOUNT_PATH = VAULT_CONFIG.kv_mount_path;
const KV_SECRET_PATHS = VAULT_CONFIG.kv_secrets_paths;
const RENEWAL_INTERVAL_SECONDS = VAULT_CONFIG.renewal_interval_seconds;
const RENEWAL_THRESHOLD_PERCENT = VAULT_CONFIG.token_renewal_threshold_percent;

// --- Vault 상태 변수 ---
let currentToken = '';
let leaseDurationSeconds = 0;
let authTimeEpochSeconds = 0;
let isRenewable = false;
let secretsCache = {};

/** 현재 토큰의 잔여 TTL을 계산하여 반환합니다. */
function getRemainingTtl() {
    const currentTimeEpoch = Math.floor(Date.now() / 1000);
    const elapsed = currentTimeEpoch - authTimeEpochSeconds;
    return leaseDurationSeconds - elapsed;
}

/** Vault API 호출을 위한 기본 Axios 인스턴스를 생성합니다. */
function getVaultClient(token = null) {
    const headers = {
        'Content-Type': 'application/json',
    };
    if (VAULT_NAMESPACE) {
        headers['X-Vault-Namespace'] = VAULT_NAMESPACE;
    }
    if (token) {
        headers['X-Vault-Token'] = token;
    }

    return axios.create({
        baseURL: VAULT_ADDR,
        headers: headers,
        validateStatus: (status) => status === 200 || status === 204, // 200 또는 204만 성공으로 간주
    });
}

// =================================
// 1. 인증: AppRole 로그인 (POST /v1/auth/approle/login)
// =================================
async function authenticate() {
    console.log("--- 🔐 Vault AppRole 인증 시작 ---");

    const url = '/v1/auth/approle/login';
    const payload = {
        role_id: ROLE_ID,
        secret_id: SECRET_ID,
    };

    try {
        const response = await getVaultClient().post(url, payload);

        if (response.status !== 200) {
            console.error(`❌ AppRole 인증 실패. HTTP Code: ${response.status}, Body: ${JSON.stringify(response.data)}`);
            throw new Error("AppRole 인증 실패");
        }

        const auth = response.data.auth;
        
        // 상태 업데이트
        currentToken = auth.client_token;
        leaseDurationSeconds = auth.lease_duration;
        isRenewable = auth.renewable;
        authTimeEpochSeconds = Math.floor(Date.now() / 1000);

        console.log("✅ Vault Auth 성공! (Auth Token 획득)");
        console.log(`   - 토큰 스트링 (일부): ${currentToken.substring(0, 10)}...`);
        console.log(`   - 토큰 lease time (TTL): ${leaseDurationSeconds} 초`);
        console.log(`   - 토큰 갱신 가능 여부: ${isRenewable}`);
        
        return true;

    } catch (error) {
        console.error(`❌ Vault AppRole 인증 중 예외 발생: ${error.message}`);
        // 치명적 오류이므로 재인증 시도하지 않음
        return false;
    }
}

// =================================
// 2. 토큰 갱신 (POST /v1/auth/token/renew-self)
// =================================
async function manualRenewToken(remainingTtl) {
    if (!isRenewable) {
        console.error("⚠️ 토큰이 갱신 불가능합니다. 재인증이 필요합니다.");
        return false;
    }

    console.log(`>>> ⚠️ 토큰 갱신 임계점 도달! 갱신 실행... (실행전 TTL: ${remainingTtl}초)`);
    
    const url = '/v1/auth/token/renew-self';
    
    try {
        const response = await getVaultClient(currentToken).post(url, {});

        if (response.status !== 200) {
            console.error(`❌ 토큰 갱신 실패. HTTP Code: ${response.status}, Body: ${JSON.stringify(response.data)}`);
            throw new Error("토큰 갱신 REST API 실패");
        }

        const auth = response.data.auth;
        
        const oldTtl = leaseDurationSeconds;
        leaseDurationSeconds = auth.lease_duration;
        authTimeEpochSeconds = Math.floor(Date.now() / 1000);
        
        console.log(">>> ✅ 토큰 갱신 성공!");
        console.log(`    - 실행후 새로운 TTL: ${leaseDurationSeconds} 초 (이전 TTL: ${oldTtl} 초)`);
        return true;

    } catch (error) {
        console.error(`❌ 토큰 갱신 중 예외 발생: ${error.message}`);
        return false;
    }
}

// =================================
// 3. KV Secret 조회 (GET /v1/<mount_path>/data/<path>)
// =================================
async function readKvSecret(secretPath) {
    if (!currentToken) {
        console.error("🛑 Secret 조회 불가: 인증 토큰이 없습니다.");
        return;
    }

    const url = `/v1/${KV_MOUNT_PATH}/data/${secretPath}`; 
    
    try {
        const response = await getVaultClient(currentToken).get(url);

        if (response.status !== 200) {
            console.error(`   - Secret 조회 실패. HTTP Code: ${response.status}, Path: ${secretPath}`);
            return;
        }

        const dataNode = response.data.data.data;
        const metadata = response.data.data.metadata;

        // 캐시 업데이트
        secretsCache[secretPath] = dataNode;
        
        const version = metadata && metadata.version ? metadata.version : 'N/A';
                             
        console.log(`   - Secret 조회/갱신 성공: ${secretPath}, Version: ${version}`);

    } catch (error) {
        console.error(`❌ KV Secret 조회 중 예외 발생 (${secretPath}): ${error.message}`);
        // 네트워크나 기타 오류 발생 시 토큰이 유효하지 않을 수 있으므로, 재인증을 시도할 수 있도록 플래그 설정
        currentToken = ''; 
    }
}

function printSecretsCache() {
    console.log("\n--- 📋 현재 Secrets Cache 내용 ---");
    for (const path in secretsCache) {
        console.log(`  [${path}]`);
        for (const key in secretsCache[path]) {
            console.log(`    - ${key}: ${secretsCache[path][key]}`);
        }
    }
    console.log("---------------------------------");
}

// =================================
// 4. 스케줄러 로직
// =================================
async function scheduledTask() {
    let isAuthenticated = !!currentToken;

    // 1. 토큰 갱신 모니터링
    // ------------------------------------------------
    if (!isAuthenticated) {
        console.log("\n🛑 토큰이 없습니다. 재인증 시도...");
        isAuthenticated = await authenticate();
    }
    
    if (isAuthenticated) {
        const remainingTtl = getRemainingTtl();
        const renewalThreshold = leaseDurationSeconds * (RENEWAL_THRESHOLD_PERCENT / 100.0);
        console.log(`\n⏳ [Token Manager] 토큰 잔여 TTL: ${Math.max(0, remainingTtl)} 초 (갱신 임계점: ${renewalThreshold} 초)`);

        if (remainingTtl <= 0) {
            console.error("🛑 토큰 만료! 재인증을 시도합니다...");
            isAuthenticated = await authenticate();
        } else if (remainingTtl <= renewalThreshold) {
            const success = await manualRenewToken(remainingTtl);
            if (!success) {
                // 갱신 실패 시 재인증 시도
                isAuthenticated = await authenticate();
            }
        } else {
            console.log("✅ TTL > 임계값. 갱신 불필요.");
        }
    } else {
        console.error("❌ AppRole 인증에 실패했습니다. 다음 주기에 재시도합니다.");
    }
    // ------------------------------------------------

    // 2. KV Secret 데이터 갱신 실행
    if (isAuthenticated) {
        console.log("\n--- ♻️ KV Secrets 갱신 스케줄러 실행 ---");
        for (const path of KV_SECRET_PATHS) {
            await readKvSecret(path);
        }
        printSecretsCache();
    } else {
        console.error("🛑 인증되지 않아 Secret 조회 스케줄러를 건너뜁니다.");
    }
}

// =================================
// 5. 메인 함수
// =================================
async function main() {
    // 1. 초기 인증
    let success = await authenticate();

    if (!success) {
        console.error("❌ 애플리케이션 초기 인증 실패. 종료합니다.");
        return;
    }

    // 2. 초기 KV Secret 로드
    console.log("\n--- 🔎 초기 KV Secrets 조회 시작 ---");
    for (const path of KV_SECRET_PATHS) {
        await readKvSecret(path);
    }
    console.log("✅ 초기 KV Secrets 조회 완료.");
    printSecretsCache();

    // 3. 스케줄러 시작
    console.log(`\n--- ♻️ KV Secrets 및 토큰 갱신 모니터링 스케쥴러 시작 (Interval: ${RENEWAL_INTERVAL_SECONDS}초) ---`);
    schedule.scheduleJob(`*/${RENEWAL_INTERVAL_SECONDS} * * * * *`, scheduledTask);
}

main().catch(error => {
    console.error(`❌ 애플리케이션 치명적 오류 발생: ${error.message}`);
});