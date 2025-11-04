using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Net.Http;
using System.Net.Http.Json;
using System.Text.Json;
using System.Threading.Tasks;
using Microsoft.Extensions.Configuration;

namespace VaultClientDotnet;

public class VaultClient
{
    private readonly VaultConfig _config;
    private readonly HttpClient _httpClient;
    private readonly JsonSerializerOptions _jsonOptions;

    // Vault 상태 변수
    private string _currentToken = string.Empty;
    private long _leaseDurationSeconds = 0;
    private long _authTimeEpochSeconds = 0;
    private bool _isRenewable = false;

    // 시크릿 저장소 (스레드 안전한 캐시)
    private readonly ConcurrentDictionary<string, Dictionary<string, string>> _secretsCache = new();

    public VaultClient(VaultConfig config)
    {
        _config = config;
        _httpClient = new HttpClient { BaseAddress = new Uri(_config.Addr) };
        _jsonOptions = new JsonSerializerOptions { PropertyNamingPolicy = JsonNamingPolicy.CamelCase };
    }

    private long GetRemainingTtl()
    {
        var currentTimeEpoch = DateTimeOffset.UtcNow.ToUnixTimeSeconds();
        var elapsed = currentTimeEpoch - _authTimeEpochSeconds;
        return _leaseDurationSeconds - elapsed;
    }
    
    // =================================
    // 1. 인증: AppRole 로그인 (POST /v1/auth/approle/login)
    // =================================
    public async Task AuthenticateAndLoadSecretsAsync()
    {
        Console.WriteLine("--- 🔐 Vault AppRole 인증 시작 ---");

        var url = "/v1/auth/approle/login";
        var payload = new { role_id = _config.RoleId, secret_id = _config.SecretId };

        var request = new HttpRequestMessage(HttpMethod.Post, url)
        {
            Content = JsonContent.Create(payload, options: _jsonOptions)
        };
        AddVaultHeaders(request);

        var response = await _httpClient.SendAsync(request);
        var responseBody = await response.Content.ReadAsStringAsync();

        if (!response.IsSuccessStatusCode)
        {
            Console.WriteLine($"❌ AppRole 인증 실패. HTTP Code: {response.StatusCode}, Body: {responseBody}");
            throw new Exception("AppRole 인증 실패");
        }

        using var document = JsonDocument.Parse(responseBody);
        var auth = document.RootElement.GetProperty("auth");

        _currentToken = auth.GetProperty("client_token").GetString() ?? string.Empty;
        _leaseDurationSeconds = auth.GetProperty("lease_duration").GetInt64();
        _isRenewable = auth.GetProperty("renewable").GetBoolean();
        _authTimeEpochSeconds = DateTimeOffset.UtcNow.ToUnixTimeSeconds();

        Console.WriteLine("✅ Vault Auth 성공! (Auth Token 획득)");
        Console.WriteLine($"   - 토큰 스트링 (일부): {_currentToken[..10]}...");
        Console.WriteLine($"   - 토큰 lease time (TTL): {_leaseDurationSeconds} 초");
        Console.WriteLine($"   - 토큰 갱신 가능 여부: {_isRenewable}");

        // 초기 KV 데이터 조회
        Console.WriteLine("\n--- 🔎 초기 KV Secrets 조회 시작 ---");
        foreach (var path in _config.KvSecretsPaths)
        {
            await ReadKvSecretAsync(path);
        }
        Console.WriteLine("✅ 초기 KV Secrets 조회 완료.");
        PrintSecretsCache();
    }
    
    // =================================
    // 2. 스케줄러 (토큰 갱신 및 KV Secret 갱신)
    // =================================
    public void StartScheduledTasks()
    {
        Console.WriteLine($"--- ♻️ KV Secrets 및 토큰 갱신 모니터링 스케쥴러 시작 (Interval: {_config.RenewalIntervalSeconds}초) ---");
        
        var intervalMs = _config.RenewalIntervalSeconds * 1000;
        
        // Timer를 사용하여 주기적인 작업 스케줄링
        var timer = new System.Threading.Timer(async _ =>
        {
            await ScheduledTaskAsync();
        }, null, intervalMs, intervalMs);
    }

    private async Task ScheduledTaskAsync()
    {
        // 1. 토큰 갱신 모니터링 및 로깅 수행
        var remainingTtl = GetRemainingTtl();
        var renewalThreshold = (long)(_leaseDurationSeconds * (_config.TokenRenewalThresholdPercent / 100.0));
        Console.WriteLine($"   - 토큰 잔여 TTL: {remainingTtl} 초 (갱신 임계점: {renewalThreshold} 초)");

        // 토큰 갱신 임계점 도달 확인 및 갱신 실행
        if (remainingTtl <= renewalThreshold && remainingTtl > 0)
        {
            try
            {
                await ManualRenewTokenAsync(remainingTtl);
            }
            catch (Exception e)
            {
                Console.WriteLine($"❌ 토큰 갱신 중 예외 발생: {e.Message}");
            }
        }

        // 2. KV Secret 데이터 갱신 실행
        Console.WriteLine("\n--- ♻️ KV Secrets 갱신 스케줄러 실행 ---");
        foreach (var path in _config.KvSecretsPaths)
        {
            try
            {
                await ReadKvSecretAsync(path);
            }
            catch (Exception e)
            {
                Console.WriteLine($"❌ KV Secret 갱신 실패 ({path}): {e.Message}");
            }
        }
        PrintSecretsCache();
    }

    /** 토큰 갱신 로직 (REST API 호출) */
    private async Task ManualRenewTokenAsync(long remainingTtl)
    {
        if (!_isRenewable)
        {
            Console.WriteLine("⚠️ 토큰이 갱신 불가능합니다. 재인증이 필요합니다.");
            return;
        }

        Console.WriteLine($">>> ⚠️ 토큰 갱신 임계점 도달! 갱신 실행... (실행전 TTL: {remainingTtl}초)");
        
        var url = "/v1/auth/token/renew-self";
        var request = new HttpRequestMessage(HttpMethod.Post, url)
        {
            Content = new StringContent("{}", System.Text.Encoding.UTF8, "application/json")
        };
        AddVaultHeaders(request, _currentToken);

        var response = await _httpClient.SendAsync(request);
        var responseBody = await response.Content.ReadAsStringAsync();

        if (!response.IsSuccessStatusCode)
        {
            Console.WriteLine($"❌ 토큰 갱신 실패. HTTP Code: {response.StatusCode}, Body: {responseBody}");
            throw new Exception("토큰 갱신 REST API 실패");
        }

        using var document = JsonDocument.Parse(responseBody);
        var auth = document.RootElement.GetProperty("auth");

        var oldTtl = _leaseDurationSeconds;
        _leaseDurationSeconds = auth.GetProperty("lease_duration").GetInt64();
        _authTimeEpochSeconds = DateTimeOffset.UtcNow.ToUnixTimeSeconds();
        
        Console.WriteLine(">>> ✅ 토큰 갱신 성공!");
        Console.WriteLine($"    - 실행후 새로운 TTL: {_leaseDurationSeconds} 초 (이전 TTL: {oldTtl} 초)");
    }

    // =================================
    // 3. KV Secret 조회 (GET /v1/<mount_path>/data/<path>)
    // =================================
    private async Task ReadKvSecretAsync(string secretPath)
    {
        var url = $"/v1/{_config.KvMountPath}/data/{secretPath}"; 
        Console.WriteLine($">>> KV Secret 요청 URL: {_httpClient.BaseAddress}{url}");

        var request = new HttpRequestMessage(HttpMethod.Get, url);
        AddVaultHeaders(request, _currentToken);

        var response = await _httpClient.SendAsync(request);
        var responseBody = await response.Content.ReadAsStringAsync();

        if (!response.IsSuccessStatusCode)
        {
            Console.WriteLine($"   - Secret 조회 실패. HTTP Code: {response.StatusCode}, Path: {secretPath}");
            return;
        }

        using var document = JsonDocument.Parse(responseBody);
        var data = document.RootElement.GetProperty("data");
        var dataNode = data.GetProperty("data"); 
        var metadata = data.GetProperty("metadata");

        var secretData = new Dictionary<string, string>();
        foreach (var property in dataNode.EnumerateObject())
        {
            secretData.Add(property.Name, property.Value.GetString() ?? string.Empty);
        }

        _secretsCache[secretPath] = secretData;
        
        var version = metadata.TryGetProperty("version", out var versionElement) 
                      ? versionElement.GetInt32().ToString() : "N/A";
                             
        Console.WriteLine($"   - Secret 조회/갱신 성공: {secretPath}, Version: {version}");
    }

    private void PrintSecretsCache()
    {
        Console.WriteLine("\n--- 📋 현재 Secrets Cache 내용 ---");
        foreach (var kvp in _secretsCache)
        {
            Console.WriteLine($"  [{kvp.Key}]");
            foreach (var dataKvp in kvp.Value)
            {
                Console.WriteLine($"    - {dataKvp.Key}: {dataKvp.Value}");
            }
        }
        Console.WriteLine("---------------------------------");
    }

    private void AddVaultHeaders(HttpRequestMessage request, string? token = null)
    {
        if (!string.IsNullOrEmpty(_config.Namespace))
        {
            request.Headers.Add("X-Vault-Namespace", _config.Namespace);
        }
        if (!string.IsNullOrEmpty(token))
        {
            request.Headers.Add("X-Vault-Token", token);
        }
    }
}

// =================================
// Program Entry Point
// =================================
internal static class Program
{
    public static async Task Main(string[] args)
    {
        // 1. 설정 로드
        var config = LoadConfiguration();
        var vaultConfig = new VaultConfig();
        config.GetSection("Vault").Bind(vaultConfig);

        var client = new VaultClient(vaultConfig);

        try
        {
            // 2. 초기 인증 및 Secret 로드
            await client.AuthenticateAndLoadSecretsAsync();

            // 3. 스케줄러 시작
            client.StartScheduledTasks();
            
            // 애플리케이션 유지를 위해 무한 대기
            Console.WriteLine("🚀 .NET Vault Client Started. Press Ctrl+C to exit.");
            await Task.Delay(Timeout.Infinite);
        }
        catch (Exception e)
        {
            Console.WriteLine($"❌ 애플리케이션 치명적 오류 발생: {e.Message}");
            Environment.Exit(1);
        }
    }

    private static IConfiguration LoadConfiguration()
    {
        return new ConfigurationBuilder()
            .SetBasePath(AppContext.BaseDirectory)
            .AddJsonFile("appsettings.json", optional: false, reloadOnChange: true)
            .Build();
    }
}