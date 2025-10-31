# Golang
## 디렉토리 구조
```
.
├── config.ini           # Vault 접속 정보 및 스케줄링 설정 파일
└── main.go              # 메인 Vault 클라이언트 로직 (인증, 갱신, Secret 조회 포함)
```


## 실행 방법
```bash
# Go 패키지 설치 - 최신버전
apt  install golang-go
apt  install gccgo-go

# Go 모듈 초기화
go mod init vault-client-go

# 의존성 설치 (HashiCorp Vault API, INI 파서)
go get github.com/hashicorp/vault/api
go get gopkg.in/ini.v1


#3 설정 파일 업데이트

#4 실행
go run main.go
```

## 결과 예시
```

```