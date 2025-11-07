# Python
## 디렉토리 구조
```
python/
├── config.ini           # Vault 접속 정보 및 설정 변수
├── requirements.txt     # Python 라이브러리 목록 (hvac, schedule)
└── vault_kv_client.py   # 메인 실행 코드
```

## 환경 구성
```bash
# 1. 시스템 업데이트
sudo dnf update -y

# 2. Python 3 및 pip 설치
# Amazon Linux 2023은 dnf를 사용합니다.
sudo dnf install -y python3 python3-pip

# 3. 버전 확인
python3 --version
pip3 --version
```

## 설정 파일 업데이트
- 수정: config.ini (각 항목을 실제 값으로 변경)
```bash
[vault]
addr = http://127.0.0.1:8200
namespace = poc-main
role_id = [실제 Role ID로 대체]  # <-- 필수 업데이트
secret_id = [실제 Secret ID로 대체] # <-- 필수 업데이트
kv_path = kv_app
 # 인증 갱신 및 조회 스케줄링 설정(기본)
interval = 10
```

## 빌드 및 실행
```bash
#1 의존성 설치/구성
pip install -r requirements.txt

#2. 실행
python3 vault_client.py
```