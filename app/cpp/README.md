# Python
## 디렉토리 구조
```
.
├── config.ini           # Vault 접속 정보 및 설정 변수
├── requirements.txt     # Python 라이브러리 목록
└── vault_client.py      # 메인 실행 코드
```

## 실행 방법
```bash
#1 설정
# config.ini 설정 (vault endpoint, rold-id, secret-id, namespace, kv path 등)

#2 의존성 설치/구성
pip install -r requirements.txt

#3 실행
python3 vault_client.py
```

## 결과 예시
```
```