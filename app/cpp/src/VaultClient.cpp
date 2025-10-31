#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <chrono>
#include <thread>
#include <algorithm>
#include <iomanip>
#include <stdexcept>

// JSON 라이브러리
#include <nlohmann/json.hpp>

// cURL 라이브러리
#include <curl/curl.h>

using json = nlohmann::json;
using namespace std::chrono;

// =========================================================
// Utility Functions
// =========================================================
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* buffer = static_cast<std::string*>(userp);
    buffer->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

// =========================================================
// Configuration Loader
// =========================================================
class Config {
private:
    std::map<std::string, std::string> properties;

    void loadFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open())
            throw std::runtime_error("❌ Error: config.properties 파일을 열 수 없습니다.");

        std::string line;
        while (std::getline(file, line)) {
            line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
            if (line.empty() || line[0] == '#') continue;

            auto eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                properties[line.substr(0, eqPos)] = line.substr(eqPos + 1);
            }
        }
    }

public:
    std::string vaultAddr;
    std::string namespaceId;
    std::string roleId;
    std::string secretId;
    std::vector<std::string> kvSecretsPaths;
    std::string kvMountPath = "kv";
    long kvRenewalIntervalSeconds = 10;
    double tokenRenewalThresholdPercent = 20.0;

    explicit Config(const std::string& filename = "config.properties") {
        std::cout << "⏳ 설정 파일 로드 중: " << filename << std::endl;
        loadFile(filename);

        vaultAddr = properties["vault.vault_addr"];
        namespaceId = properties["vault.vault_namespace"];
        roleId = properties["vault.vault_role_id"];
        secretId = properties["vault.vault_secret_id"];
        if (properties.count("kv_mount_path")) kvMountPath = properties["kv_mount_path"];

        try {
            kvRenewalIntervalSeconds = std::stol(properties["kv_renewal_interval_seconds"]);
            tokenRenewalThresholdPercent = std::stod(properties["token_renewal_threshold_percent"]);
        } catch (...) {
            throw std::runtime_error("❌ Error: 설정 파일의 숫자 값을 파싱할 수 없습니다.");
        }

        std::stringstream ss(properties["kv_secrets_paths"]);
        for (std::string path; std::getline(ss, path, ',');) {
            path.erase(std::remove_if(path.begin(), path.end(), ::isspace), path.end());
            if (!path.empty()) kvSecretsPaths.push_back(path);
        }

        std::cout << "✅ 설정 파일 로드 완료. Vault Addr: " << vaultAddr << std::endl;
    }
};

// =========================================================
// Vault Client
// =========================================================
class VaultClient {
private:
    Config config;
    std::string currentToken;
    long leaseDurationSeconds = 0;
    long authTimeEpochSeconds = 0;
    bool isRenewable = false;
    std::map<std::string, std::map<std::string, std::string>> secretsCache;
    CURL* curl = nullptr;

    // ---------------------------------------------------------
    // HTTP POST
    // ---------------------------------------------------------
    long executePost(const std::string& url, const std::string& payload, const std::string& token, std::string& response) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        if (!config.namespaceId.empty())
            headers = curl_slist_append(headers, ("X-Vault-Namespace: " + config.namespaceId).c_str());
        if (!token.empty())
            headers = curl_slist_append(headers, ("X-Vault-Token: " + token).c_str());

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        CURLcode res = curl_easy_perform(curl);

        long httpCode = 0;
        if (res == CURLE_OK)
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        else
            std::cerr << "❌ CURL Error: " << curl_easy_strerror(res) << std::endl;

        curl_slist_free_all(headers);
        return httpCode;
    }

    // ---------------------------------------------------------
    // HTTP GET
    // ---------------------------------------------------------
    long executeGet(const std::string& url, const std::string& token, std::string& response) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        struct curl_slist* headers = nullptr;
        if (!config.namespaceId.empty())
            headers = curl_slist_append(headers, ("X-Vault-Namespace: " + config.namespaceId).c_str());
        if (!token.empty())
            headers = curl_slist_append(headers, ("X-Vault-Token: " + token).c_str());

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        if (res == CURLE_OK)
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        else
            std::cerr << "❌ CURL Error: " << curl_easy_strerror(res) << std::endl;

        curl_slist_free_all(headers);
        return httpCode;
    }

    // ---------------------------------------------------------
    // AppRole 인증
    // ---------------------------------------------------------
    void authenticate() {
        std::cout << "\n🔐 Vault AppRole 인증 중..." << std::endl;
        const auto url = config.vaultAddr + "/v1/auth/approle/login";
        const json payload = {{"role_id", config.roleId}, {"secret_id", config.secretId}};
        std::string response;

        const auto httpCode = executePost(url, payload.dump(), "", response);
        if (httpCode != 200)
            throw std::runtime_error("AppRole 인증 실패: " + std::to_string(httpCode) + " → " + response.substr(0, 100));

        const auto root = json::parse(response);
        const auto& auth = root.at("auth");

        currentToken = auth.at("client_token").get<std::string>();
        leaseDurationSeconds = auth.at("lease_duration").get<long>();
        isRenewable = auth.at("renewable").get<bool>();
        authTimeEpochSeconds = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();

        std::cout << "✅ 인증 성공: TTL=" << leaseDurationSeconds
                  << "초, Renewable=" << (isRenewable ? "true" : "false") << std::endl;
    }

    // ---------------------------------------------------------
    // 토큰 갱신
    // ---------------------------------------------------------
    void renewToken(long remainingTtl) {
        if (!isRenewable) {
            std::cerr << "⚠️ 현재 토큰은 갱신 불가. 재인증 필요." << std::endl;
            return;
        }

        std::cout << "♻️ 토큰 갱신 시도 (잔여 TTL=" << remainingTtl << "초)" << std::endl;
        const auto url = config.vaultAddr + "/v1/auth/token/renew-self";
        std::string response;

        const auto httpCode = executePost(url, "{}", currentToken, response);
        if (httpCode != 200)
            throw std::runtime_error("토큰 갱신 실패: " + std::to_string(httpCode));

        const auto root = json::parse(response);
        const auto& auth = root.at("auth");

        const auto oldTtl = leaseDurationSeconds;
        leaseDurationSeconds = auth.at("lease_duration").get<long>();
        authTimeEpochSeconds = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();

        std::cout << "✅ 토큰 갱신 성공: 새 TTL=" << leaseDurationSeconds << " (이전=" << oldTtl << ")" << std::endl;
    }

    // ---------------------------------------------------------
    // KV Secret 조회
    // ---------------------------------------------------------
    void readKvSecret(const std::string& secretPath) {
        const auto url = config.vaultAddr + "/v1/" + config.kvMountPath + "/data/" + secretPath;
        std::string response;
        const auto httpCode = executeGet(url, currentToken, response);

        if (httpCode != 200) {
            std::cerr << "❌ Secret 조회 실패: " << secretPath << " (HTTP " << httpCode << ")" << std::endl;
            return;
        }

        const auto root = json::parse(response);
        const auto& dataNode = root.at("data").at("data");
        const auto& metadata = root.at("data").at("metadata");

        std::map<std::string, std::string> secretData;
        for (const auto& [key, val] : dataNode.items()) {
            if (val.is_string())
                secretData[key] = val.get<std::string>();
            else
                secretData[key] = val.dump();
        }

        secretsCache[secretPath] = std::move(secretData);

        std::string versionStr;
        try {
            const auto& versionNode = metadata.at("version");
            versionStr = versionNode.is_number() ? std::to_string(versionNode.get<int>()) : versionNode.get<std::string>();
        } catch (...) {
            versionStr = "N/A";
        }

        std::cout << "✅ Secret 갱신 완료: " << secretPath << " (Version=" << versionStr << ")" << std::endl;
    }

    long getRemainingTtl() const {
        const auto now = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
        return leaseDurationSeconds - (now - authTimeEpochSeconds);
    }

    void printSecretsCache() const {
        std::cout << "\n📋 [Secrets Cache]" << std::endl;
        for (const auto& [path, kvPairs] : secretsCache) {
            std::cout << "  [" << path << "]" << std::endl;
            for (const auto& [k, v] : kvPairs)
                std::cout << "    " << k << ": " << v << std::endl;
        }
        std::cout << "-------------------------------\n" << std::endl;
    }

public:
    VaultClient() : config("config.properties") {
        curl = curl_easy_init();
        if (!curl) throw std::runtime_error("❌ cURL 초기화 실패.");
    }

    ~VaultClient() {
        if (curl) curl_easy_cleanup(curl);
        curl_global_cleanup();
    }

    void run() {
        authenticate();

        std::cout << "\n🔎 초기 KV Secrets 조회..." << std::endl;
        for (const auto& path : config.kvSecretsPaths)
            readKvSecret(path);
        printSecretsCache();

        const auto interval = config.kvRenewalIntervalSeconds;
        const auto renewalThreshold = static_cast<long>(
            config.tokenRenewalThresholdPercent / 100.0 * leaseDurationSeconds);

        std::cout << "\n♻️ 주기적 토큰/Secret 갱신 시작 (Interval=" << interval << "s)" << std::endl;

        while (true) {
            std::this_thread::sleep_for(seconds(interval));

            const auto remainingTtl = getRemainingTtl();
            std::cout << "⏱️ 현재 토큰 TTL: " << remainingTtl << "초" << std::endl;

            if (remainingTtl <= renewalThreshold && remainingTtl > 0) {
                try {
                    renewToken(remainingTtl);
                } catch (const std::exception& e) {
                    std::cerr << "❌ 토큰 갱신 오류: " << e.what() << std::endl;
                }
            }

            for (const auto& path : config.kvSecretsPaths)
                readKvSecret(path);
            printSecretsCache();
        }
    }
};

// =========================================================
// main()
// =========================================================
int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    try {
        VaultClient client;
        client.run();
    } catch (const std::exception& e) {
        std::cerr << "❌ VaultClient 실행 오류: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}