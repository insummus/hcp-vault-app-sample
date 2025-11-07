# Java
## 디렉토리 구조
```
java/
├── pom.xml              # Maven 프로젝트 파일 (의존성 정의)
└── src
    └── main
        ├── java
        │   └── com/example/vault/client/
        │       ├── VaultHttpClient.java  (메인 로직)
        │       └── VaultConfig.java      (설정 로드)
        └── resources
            ├── config.properties   (설정 파일)
            └── logback.xml         (로깅 설정)
```


## 환경 구성
```bash
# 1. 시스템 업데이트
sudo dnf update -y

# 2. OpenJDK 17 설치
# Maven 프로젝트 빌드 및 실행을 위해 JDK 17을 설치합니다.
sudo dnf install -y java-17-amazon-corretto-devel

# 3. Maven 설치
# 프로젝트 빌드를 위한 Maven을 설치합니다.
sudo dnf install -y maven

# 4. 버전 확인
java -version
mvn -v
```

## 설정 파일 업데이트
- 수정: src/main/resources/config.properties (각 항목을 실제 값으로 변경)
```bash
vault.vault_addr = http://127.0.0.1:8200
vault.vault_namespace = poc-main
vault.vault_role_id = 
vault.vault_secret_id = 
kv_mount_path = kv_app
kv_secrets_paths = application

# 인증 갱신 및 조회 스케줄링 설정(기본)
kv_renewal_interval_seconds = 10
token_renewal_threshold_percent = 20
```

## 빌드 및 실행
```bash
# pom.xml 위치
# 1. 애플리케이션 빌드
sudo mvn clean install

# 2. 애플리케이션 실행
# JAR 파일명은 프로젝트 버전에 따라 달라질 수 있습니다.
sudo java -jar target/vault-client-1.0-SNAPSHOT.jar
```