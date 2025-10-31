# 파일명: Makefile

# 컴파일러 설정
CC = gcc

# 출력 파일명
TARGET = vault_client_app

# 소스 파일 디렉토리 및 파일 (경로 지정)
SRC_DIR = src
SRC = $(SRC_DIR)/vault_client.c

# 필요한 라이브러리 (Dependencies)
# -lcurl: libcurl 라이브러리 링크
# -ljansson: jansson 라이브러리 링크
# -pthread: POSIX 스레드 라이브러리 링크
LDLIBS = -lcurl -ljansson -pthread

# 컴파일 플래그
# -Wall: 모든 경고 활성화
# -std=c99: C99 표준 사용
CFLAGS = -Wall -std=c99

# -----------------
# 기본 타겟: 빌드 (make all 또는 make)
# -----------------
all: $(TARGET)

# 실행 파일 빌드 규칙
# $< : 첫 번째 종속성 (src/vault_client.c)
# $@ : 타겟 이름 (vault_client_app)
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

# -----------------
# 클린 타겟 (make clean)
# -----------------
clean:
	rm -f $(TARGET)
	@echo "Cleaned up build files."

.PHONY: all clean