# Java(Kotlin)
## 디렉토리 구조
```
vault-client-kotlin/
├── build.gradle.kts     # Gradle Kotlin 빌드 파일
└── src/
    └── main/
        ├── kotlin/
        │   └── com/example/vault/client/
        │       ├── VaultConfig.kt
        │       └── VaultClient.kt      # 메인 로직
        └── resources/
            ├── application.properties  # 설정 파일
            └── logback.xml
```


## 실행 방법
```bash
# 디렉토리 생성
mkdir -p vault-client-kotlin/src/main/{kotlin/com/example/vault/client,resources}


# 패키지 설치
# 1. 시스템 패키지 업데이트
sudo apt update

# 2. OpenJDK 17 설치 (Java 17 이상 권장)
sudo apt install openjdk-17-jdk

# 버전 확인
sudo javac -version
sudo java -version

# 3. Gradle 설치 (Ubuntu/Debian 환경 기준)
sudo apt install gradle



# 의존성 설치/구성
npm install

# 실행
npm start
```

## 결과 예시
```

```