# .NET(C#)
## 디렉토리 구조
```
dotnet-cs
├── appsettings.json         # Vault 접속 및 스케줄링 설정 파일
├── VaultClient.csproj       # .NET 프로젝트 파일 (의존성 정의)
├── VaultClient.cs           # 메인 Vault 클라이언트 로직 및 Entry Point
└── VaultConfig.cs           # 설정 클래스 (C# 객체)
```

# Java(Kotlin)
## 디렉토리 구조
```
java-kotlin/
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
# 패키지 설치
# 1. 시스템 업데이트
sudo dnf update -y

# 2. .NET 8.0 SDK 설치
sudo dnf install -y dotnet-sdk-8.0

# 3. 버전 확인
dotnet --version
```

## 설정 파일 업데이트
- 수정: appsettings.json (각 항목을 실제 값으로 변경)
```
{
  "Vault": {
    "Addr": "http://127.0.0.1:8200",
    "Namespace": "poc-main",
    "RoleId": "[여기에 실제 Role ID 입력]", 
    "SecretId": "[여기에 실제 Secret ID 입력]",
    "KvMountPath": "kv_app",
    "KvSecretsPaths": [ "application" ],
    
    # 인증 갱신 및 조회 스케줄링 설정(기본)
    "RenewalIntervalSeconds": 10,
    "TokenRenewalThresholdPercent": 20
  }
}
```

## 빌드 및 실행
```bash
# 1. 프로젝트 디렉토리로 이동 (예시)
cd dotnet-cs

# 2. 의존성 복원 및 빌드
dotnet publish -c Release -o ./publish

# 3. 애플리케이션 실행
cd publish
dotnet VaultClient.dll
```