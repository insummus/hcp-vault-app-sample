cmake_minimum_required(VERSION 3.14)
project(VaultClientCpp VERSION 1.0)

# ✅ C++17 이상 사용
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# nlohmann/json 라이브러리 자동 다운로드
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# 실행 파일 정의
add_executable(vault_client src/VaultClient.cpp)

# 라이브러리 연결
find_package(CURL REQUIRED)
target_link_libraries(vault_client PRIVATE CURL::libcurl nlohmann_json)