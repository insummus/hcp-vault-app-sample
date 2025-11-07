# Cpp
## 디렉토리 구조
```
cpp/
├── CMakeLists.txt       # CMake 빌드 설정 파일 (종속성 정의 포함)
├── config.properties    # Vault 접속 정보 및 설정 변수
└── src/
    └── VaultClient.cpp  # 메인 Vault 클라이언트 로직
```

## 환경 구성
```bash
# 1. 시스템 업데이트 및 빌드 필수 도구 (gcc-c++, cmake) 설치
sudo dnf update -y
sudo dnf install -y cmake gcc-c++

# 2. Vault 통신 라이브러리 (libcurl) 설치
# Vault API 통신을 위한 libcurl 개발 파일을 설치합니다.
sudo dnf install -y libcurl-devel

# 참고: JSON 라이브러리(nlohmann/json)는 CMake 파일에 명시된 FetchContent 모듈을 통해 빌드 시 자동으로 다운로드됩니다.
```

## 설정 파일 업데이트
- 수정: config.properties (각 항목을 실제 값으로 변경)
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
# 1. 빌드 디렉토리 생성 및 이동 (소스 디렉토리 밖에서 빌드하는 것이 일반적입니다)
mkdir build
cd build

# 2. CMake 구성 및 종속성 다운로드
# (CMakeLists.txt에 따라 nlohmann/json 다운로드 및 프로젝트 파일 생성)
cmake ..

# 3. 애플리케이션 빌드
# (실행 파일명은 CMakeLists.txt에 정의된 대로 vault_client입니다)
make

# 4. 애플리케이션 실행
./vault_client
```