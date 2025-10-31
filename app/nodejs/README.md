# Nodejs
## 디렉토리 구조
```

```


## 실행 방법
```
# 사전 설치/구성 - 패키지/라이브러리
sudo apt udpate

## OpenJDK 17 설치
sudo apt install openjdk-17-jdk
sudo javac -version
sudo java -version

## maven 설치
sudo apt install maven
```

```
# 의존성 설치/구성(Makefile 위치)
mvn clean install -DskipTests

# config.ini 설정 (vault endpoint, rold-id, secret-id, namespace, kv path 등)
[VAULT]
ADDR = http://127.0.0.1:8200
NAMESPACE = nxk-poc
ROLE_ID = eb4eb3c8-5eaf-e2da-02f2-16a9b1828023
SECRET_ID = 186916aa-6b0c-549e-cc89-7be91774ff10
KV_MOUNT_POINT = nxk-kv
KV_SECRET_PATH = application
RENEWAL_THRESHOLD_RATIO = 0.2
SECRET_INTERVAL_SECONDS = 10
TOKEN_TTL_SECONDS_ASSUMED = 120

# 실행

```

## 결과 예시
```

```