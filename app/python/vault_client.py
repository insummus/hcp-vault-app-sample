import hvac
import schedule
import time
import logging
import configparser

# 로깅 설정: %(asctime)s를 제거하여 Node.js 스타일처럼 메시지만 출력
logging.basicConfig(level=logging.INFO, format='%(message)s')
logger = logging.getLogger()

# config.ini에서 값 읽기
config = configparser.ConfigParser()
config.read('config.ini')

VAULT_ADDR = config['vault']['addr']
VAULT_NAMESPACE = config['vault']['namespace']
ROLE_ID = config['vault']['role_id']
SECRET_ID = config['vault']['secret_id']
KV_PATH = config['vault']['kv_path']
INTERVAL = int(config['vault']['interval'])

client = hvac.Client(url=VAULT_ADDR, namespace=VAULT_NAMESPACE)

def vault_login():
    logger.info("--- 🔐 Vault AppRole 인증 시작 ---")
    auth = client.auth.approle.login(role_id=ROLE_ID, secret_id=SECRET_ID)
    client.token = auth['auth']['client_token']
    
    logger.info("✅ Vault Auth 성공! (Auth Token 획득)")
    logger.info(f"   - 토큰 스트링 (일부): {client.token[:10]}...")
    logger.info(f"   - 토큰 lease time (TTL): {auth['auth']['lease_duration']} 초")
    logger.info(f"   - 토큰 갱신 가능 여부: {auth['auth']['renewable']}")
    return auth['auth']

def log_token_status():
    tk = client.auth.token.lookup_self()
    ttl = tk['data']['ttl']
    renewable = tk['data']['renewable']
    max_ttl = tk['data']['creation_ttl']
    # 토큰 상태를 간결하게 출력
    logger.info(f"⏳ 토큰 잔여 TTL: {ttl}초 (갱신 임계점: {int(max_ttl * 0.2)}초, Max TTL: {max_ttl}초)")
    return ttl, max_ttl, renewable

def check_and_renew_token():
    ttl, max_ttl, renewable = log_token_status()
    
    renewal_threshold = max_ttl * 0.2
    
    if ttl < renewal_threshold and renewable:
        logger.info("🚨 토큰 TTL 20% 이하, 토큰 자동 갱신 시도")
        before_ttl = ttl
        result = client.auth.token.renew_self()
        after_ttl = result['auth']['lease_duration']
        logger.info(f"✅ 토큰 Renewal 완료: 새 TTL={after_ttl}s (이전={before_ttl}s)")
    elif not renewable:
        logger.info("⚠️ 토큰이 renewable하지 않음. 재인증 실행")
        vault_login()
    else:
        logger.info("✅ TTL > 임계값. 갱신 불필요.")


def read_kv_secrets():
    logger.info("\n--- ♻️ KV Secrets 갱신 스케줄러 실행 ---")
    try:
        # 토큰 갱신 체크를 먼저 수행합니다.
        check_and_renew_token() 
        
        for secret_name in ['application']:
            resp = client.secrets.kv.v2.read_secret_version(
                path=secret_name,
                mount_point=KV_PATH,
                raise_on_deleted_version=True
            )
            # Secret 출력 형식을 구조화
            logger.info(f"✅ Secret 조회/갱신 성공: {secret_name}")
            
            # 딕셔너리 내용을 개별적으로 출력하여 가독성 향상
            secret_data = resp['data']['data']
            logger.info(f"   - Secret Data:")
            for key, value in secret_data.items():
                 logger.info(f"     - {key}: {value}")
                 
        logger.info("-------------------------------\n")
        
    except Exception as e:
        logger.error(f"❌ Secret 조회 에러: {e}")
        vault_login()


if __name__ == "__main__":
    vault_login()
    
    # 초기 Secret 조회 (다른 클라이언트 예제와의 흐름 일치)
    logger.info("\n--- 🔎 초기 KV Secrets 조회 시작 ---")
    read_kv_secrets()
    
    logger.info(f"--- ♻️ KV Secrets 및 토큰 갱신 모니터링 스케쥴러 시작 (Interval: {INTERVAL}초) ---") 
    
    # 주기적 스케줄링
    schedule.every(INTERVAL).seconds.do(read_kv_secrets)
    while True:
        schedule.run_pending()
        time.sleep(1)