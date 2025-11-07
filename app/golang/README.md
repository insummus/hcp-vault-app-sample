# Golang
## 디렉토리 구조
```
golang
├── config.ini           # Vault 접속 정보 및 스케줄링 설정 파일
├── main.go              # 메인 Vault 클라이언트 로직 (인증, 갱신, Secret 조회 포함)
├── go.mod               # Go 모듈 및 종속성 정의
└── go.sum               # 종속성 체크섬 파일
```

## 환경 구성
```bash
# 1. 시스템 업데이트
sudo dnf update -y

# 2. Go 언어 환경 설치
# Amazon Linux 2023에서 Go를 설치합니다.
sudo dnf install -y golang 

# 3. Go 버전 확인
go version
```

## 설정 파일 업데이트
- 수정: config.ini(각 항목을 실제 값으로 변경)
```bash
[vault]
addr = http://127.0.0.1:8200
namespace = poc-main
role_id = 
secret_id = 
kv_path = kv_app
kv_secrets_paths = application

# 인증 갱신 및 조회 스케줄링 설정(기본)
renewal_interval_seconds = 10
token_renewal_threshold_percent = 20
```

## 빌드 및 실행
```bash
# 1.의존성 설치
go get github.com/hashicorp/vault/api
go get gopkg.in/ini.v1

# 2. 모듈 정리 및 종속성 다운로드
# # Go 모듈 초기화: go mod init vault-client-go
go mod tidy

# 3. 애플리케이션 실행
go run main.go
```