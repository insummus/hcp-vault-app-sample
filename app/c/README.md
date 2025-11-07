# C
## 디렉토리 구조
```
c/
├── Makefile             # 빌드 명령 정의 (src 경로 포함)
├── config.ini           # 설정 파일
└── src
    └── vault_client.c   # 메인 C 코드
```

## 환경 구성
```bash
# 1. 시스템 업데이트 및 빌드 필수 도구 (gcc, make 등) 설치
sudo dnf update -y
sudo dnf install -y gcc make

# 2. Vault 통신 및 JSON 처리에 필요한 개발 라이브러리 설치
# - libcurl-devel: Vault REST API 호출을 위한 libcurl 개발 파일
# - jansson-devel: JSON 응답 처리를 위한 jansson 개발 파일
sudo dnf install -y libcurl-devel jansson-devel
```

## 설정 파일 업데이트
```bash
- 수정: config.ini (각 항목을 실제 값으로 변경)
[VAULT]
ADDR = http://127.0.0.1:8200
NAMESPACE = poc-main
ROLE_ID = ""
SECRET_ID = ""
KV_MOUNT_POINT = ""
KV_SECRET_PATH = ""

# 인증 갱신 및 조회 스케줄링 설정(기본)
SECRET_INTERVAL_SECONDS = 10
RENEWAL_THRESHOLD_RATIO = 0.2 #(20%)
TOKEN_TTL_SECONDS_ASSUMED = 120
```

## 빌드 및 실행
```bash
#1 의존성 설치/구성(Makefile 위치)
make

#3 실행
./vault_client_app 
```