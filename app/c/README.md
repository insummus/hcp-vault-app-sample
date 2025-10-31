# C
## 디렉토리 구조
```
.
├── Makefile             # 빌드 명령 정의 (src 경로 포함)
├── config.ini           # 설정 파일
└── src
    └── vault_client.c   # 메인 C 코드
```


## 실행 방법
```bash
#1 config.ini 설정 (vault endpoint, namespace, rold-id, secret-id, kv path 등)
{config.ini 파일 업데이트}

#2 의존성 설치/구성(Makefile 위치)
make

#3 실행
./vault_client_app 
```

## 결과 예시
```
```