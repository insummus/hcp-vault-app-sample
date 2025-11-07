# Stage 1: Build (SDK 환경)
# .NET SDK 8.0을 사용하여 애플리케이션을 빌드합니다.
FROM mcr.microsoft.com/dotnet/sdk:8.0 AS build
WORKDIR /src

# 프로젝트 파일 복사 및 의존성 복원
COPY VaultClient.csproj .
RUN dotnet restore

# 나머지 소스 코드 복사 및 릴리즈 빌드
COPY . .
RUN dotnet publish -c Release -o /app/publish

# Stage 2: Final (런타임 환경)
# .NET ASP.NET 8.0 런타임 환경을 기반으로 합니다. (경량화된 Linux 환경)
FROM mcr.microsoft.com/dotnet/aspnet:8.0 AS final
WORKDIR /app
EXPOSE 8200 

# 빌드 스테이지에서 생성된 최종 출력 파일(dll, appsettings.json 등) 복사
COPY --from=build /app/publish .
# appsettings.json은 빌드 결과에 포함되므로 이 명령어는 필요 없을 수도 있지만, 안전을 위해 유지합니다.
COPY appsettings.json .

# 컨테이너 시작 시 실행될 명령어 정의
# VaultClient.dll을 실행합니다.
ENTRYPOINT ["dotnet", "VaultClient.dll"]