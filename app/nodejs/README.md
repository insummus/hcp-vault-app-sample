# Nodejs
## 디렉토리 구조
```
nodejs/
├── config/
│   └── default.json     # Vault 접속 정보 및 스케줄링 설정
├── package.json         # Node.js 패키지 정보 및 의존성 목록
└── vaultClient.js       # 메인 실행 로직
```


## 환경 구성
```bash
# 1. 시스템 업데이트
sudo dnf update -y

# 2. Node.js 및 npm 설치 (Amazon Linux 2023은 dnf를 사용합니다)
sudo dnf install -y nodejs

# 3. 버전 확인
node -v
npm -v
```

## 설정 파일 업데이트
- 수정: config/default.json (각 항목을 실제 값으로 변경)
```bash
{
    "vault": {
        "addr": "http://127.0.0.1:8200",
        "namespace": "poc-main",
        "role_id": "[실제 Role ID로 대체]",    
        "secret_id": "[실제 Secret ID로 대체]",
        "kv_mount_path": "kv_app",
        "kv_secrets_paths": [
            "application"
        ],
        # 인증 갱신 및 조회 스케줄링 설정(기본)
        "renewal_interval_seconds": 10,
        "token_renewal_threshold_percent": 20
    }
}
```

## 빌드 및 실행
```bash
# 1. 의존성 설치
npm install

# 2. 애플리케이션 실행
npm start
```



## 실행 방법
```bash
# 패키지 설치
apt install npm

# 의존성 설치/구성
npm install

# 실행
npm start
```

## 결과 예시
```

```