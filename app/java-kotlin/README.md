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


## 환경 구성
```bash
# 디렉토리 생성
mkdir -p vault-kotlin/src/main/{kotlin/com/example/vault/client,resources}


# 패키지 설치
# 1. 시스템 패키지 업데이트
sudo dnf update -y

# 2. OpenJDK 17 설치 (Amazon Corretto 17)
# Kotlin 프로젝트의 JVM 타겟 버전이 17로 설정되어 있으므로 이를 설치합니다.
sudo dnf install java-17-amazon-corretto-devel -y

# 3. Gradle 설치(수동))
# 빌드를 위해 Gradle이 필요합니다.
# 3-1. 필수 유틸리티 설치 (wget, unzip이 이미 설치되어 있을 수 있지만, 안전을 위해 설치)
sudo dnf install wget unzip -y

# 3-2. 설치 디렉토리 생성
sudo mkdir -p /opt/gradle

# 3-3. Gradle 바이너리 다운로드 (최신 안정화 버전 사용)
# 현재 시점의 최신 버전(9.2.0)을 기준으로 합니다.
GRADLE_VERSION="9.2.0"
wget https://services.gradle.org/distributions/gradle-${GRADLE_VERSION}-bin.zip

# 3-4. 다운로드한 파일 압축 해제 및 /opt/gradle로 이동
sudo unzip -d /opt/gradle gradle-${GRADLE_VERSION}-bin.zip

# 3-5. 환경 변수 설정 스크립트 생성
# Gradle 홈 경로와 실행 파일 경로를 시스템 PATH에 추가합니다.
echo "export GRADLE_HOME=/opt/gradle/gradle-${GRADLE_VERSION}" | sudo tee /etc/profile.d/gradle.sh
echo "export PATH=\$PATH:\$GRADLE_HOME/bin" | sudo tee -a /etc/profile.d/gradle.sh

# 3-6. 환경 변수 적용 (현재 쉘에 적용)
source /etc/profile.d/gradle.sh


# 4. 버전 확인
java -version
gradle -v
```

## 설정 파일 업데이트
- 수정: src/main/resources/application.properties 
```
vault {
  addr = "http://127.0.0.1:8200" # <--- 이 부분을 Vault Endpoint로 변경
  namespace = "poc-main"

  role_id = ""  # <--- 이 부분을 실제 Role ID로 변경
  secret_id = "" # <--- 이 부분을 실제 Secret ID로 변경

  kv_mount_path = "kv_app" # <--- 이 부분을 실제 값으로 변경
  kv_secrets_paths = [
    "application" # <--- 이 부분을 실제 값으로 변경
  ]

  renewal_interval_seconds = 10
  token_renewal_threshold_percent = 20
}
```

## 빌드 및 실행
```bash
# 1. 프로젝트 디렉토리로 이동 (예시 경로)
# 현재 디렉토리가 build.gradle.kts가 있는 위치라고 가정
cd vault-client-kotlin/

# 2. 빌드 (의존성 다운로드 및 실행 가능한 JAR 파일 생성)
# 'installDist' 태스크는 실행 스크립트와 JAR 파일을 'build/install' 디렉토리에 생성합니다.
gradle clean installDist

# 또는 'assemble' 태스크를 사용하여 build/libs/vault-client-kotlin-1.0-SNAPSHOT.jar를 생성할 수도 있습니다.
# gradle build

# 3. 실행
# installDist로 생성된 실행 스크립트를 사용하여 애플리케이션을 실행합니다.
./build/install/java-kotlin/bin/java-kotlin

# 또는 build/libs에 생성된 JAR 파일을 직접 실행할 수도 있습니다.
# JAR 파일명은 프로젝트 버전(1.0-SNAPSHOT)에 따라 달라질 수 있습니다.
java -jar build/libs/vault-client-kotlin-1.0-SNAPSHOT.jar
```
