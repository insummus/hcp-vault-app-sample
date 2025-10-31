# Java
## 디렉토리 구조
```
vault-client
├── pom.xml
└── src
    └── main
        ├── java
        │   └── com
        │       └── example
        │           └── vault
        │               └── client
        │                   ├── VaultHttpClient.java  (메인 로직)
        │                   └── VaultConfig.java      (설정 로드)
        └── resources
            ├── config.properties   (설정 파일)
            └── logback.xml         (로깅 설정)
```


## 실행 방법
```bash
#1 사전 설치/구성 - 패키지/라이브러리
sudo apt udpate

## OpenJDK 17 설치
sudo apt install openjdk-17-jdk
sudo javac -version
sudo java -version

#2 maven 설치
sudo apt install maven
```

```bash
#1 디렉토리 생성
## Maven 표준 및 패키지 구조 생성
mkdir -p src/main/java/com/example/vault/client
mkdir -p src/main/resources

#2 필수 파일 생성
touch pom.xml
touch src/main/resources/config.properties
touch src/main/resources/logback.xml
touch src/main/java/com/example/vault/client/VaultConfig.java
touch src/main/java/com/example/vault/client/VaultHttpClient.java

#3 설정 파일 업데이트
#    - pom.xml (Apache HttpClient, Jackson, Logback 의존성)
#    - config.properties (AppRole ID/Secret, kv_mount_path=kv 포함)
#    - logback.xml (로깅 설정, 이전 답변 참조)
#    - VaultConfig.java 및 VaultHttpClient.java (아래 코드 참조)

#4 실행
## 의존성 설치/구성(pom.xml 위치)
sudo mvn clean install 
sudo java -jar target/vault-client-1.0-SNAPSHOT.jar

```

## 결과 예시
```

```