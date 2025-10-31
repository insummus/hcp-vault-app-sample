import hvac
import schedule
import time
import logging
import configparser

# 로깅 설정
logging.basicConfig(level=logging.INFO, format='%(asctime)s %(message)s')
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
    logger.info("Vault AppRole 인증 시작")
    auth = client.auth.approle.login(role_id=ROLE_ID, secret_id=SECRET_ID)
    client.token = auth['auth']['client_token']
    logger.info(f"인증 성공 - token: {client.token}")
    logger.info(f"토큰 lease TTL: {auth['auth']['lease_duration']}s, renewable: {auth['auth']['renewable']}")
    return auth['auth']

def log_token_status():
    tk = client.auth.token.lookup_self()
    ttl = tk['data']['ttl']
    renewable = tk['data']['renewable']
    max_ttl = tk['data']['creation_ttl']
    logger.info(f"토큰 TTL: {ttl}s, Max TTL: {max_ttl}s, renewable: {renewable}")
    return ttl, max_ttl, renewable

def check_and_renew_token():
    ttl, max_ttl, renewable = log_token_status()
    if ttl < max_ttl * 0.2 and renewable:
        logger.info("토큰 TTL 20% 이하, 토큰 자동 갱신 시도")
        before_ttl = ttl
        result = client.auth.token.renew_self()
        after_ttl = result['auth']['lease_duration']
        logger.info(f"토큰 Renewal 완료: 이전 TTL={before_ttl}s, 갱신 TTL={after_ttl}s")
    elif not renewable:
        logger.info("토큰이 renewable하지 않음. 재인증 실행")
        vault_login()

def read_kv_secrets():
    try:
        check_and_renew_token()
        for secret_name in ['application']:
            resp = client.secrets.kv.v2.read_secret_version(
                path=secret_name,
                mount_point=KV_PATH,
                raise_on_deleted_version=True
            )
            logger.info(f"[{secret_name}] Secret: {resp['data']['data']}")
    except Exception as e:
        logger.error(f"Secret 조회 에러: {e}")
        vault_login()


if __name__ == "__main__":
    vault_login()
    logger.info(f"Vault secrets polling 시작 (interval={INTERVAL}s)")
    schedule.every(INTERVAL).seconds.do(read_kv_secrets)
    while True:
        schedule.run_pending()
        time.sleep(1)