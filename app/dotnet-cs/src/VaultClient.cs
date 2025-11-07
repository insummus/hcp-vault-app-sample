using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Net.Http;
using System.Net.Http.Json;
using System.Text.Json;
using System.Threading.Tasks;
using System.Threading;
using Microsoft.Extensions.Configuration;
using System.IO;
using System.Linq; 

namespace NewVaultClientDotnet;

public class VaultClient
{
    private readonly VaultConfig _config;
    private readonly HttpClient _httpClient;

    // Vault 상태 변수 
    private volatile string _currentToken = string.Empty;
    private long _leaseDurationSeconds = 0; 
    private long _authTimeEpochSeconds = 0; 
    private volatile bool _isRenewable = false;

    // 시크릿 저장소 (스레드 안전한 캐시)
    private readonly ConcurrentDictionary<string, Dictionary<string, string>> _secretsCache = new();
    
    public VaultClient(VaultConfig config)
    {
        _config = config;
        _httpClient = new HttpClient { BaseAddress = new Uri(_config.Addr) };
    }

    private long GetRemainingTtl()
    {
        var currentTimeEpoch = DateTimeOffset.UtcNow.ToUnixTimeSeconds();
        var elapsed = currentTimeEpoch - _authTimeEpochSeconds;
        return Math.Max(0, _leaseDurationSeconds - elapsed);
    }

    // Vault 공통 헤더 추가
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

    // 1. AppRole 인증 (POST /v1/auth/approle/login)
    public async Task AuthenticateAsync()
    {
        Console.WriteLine("--- 🔐 Vault AppRole 인증 시작 ---");

        var url = "/v1/auth/approle/login";
        var payload = new { role_id = _config.RoleId, secret_id = _config.SecretId };
        var jsonPayload = JsonSerializer.Serialize(payload);
        
        var request = new HttpRequestMessage(HttpMethod.Post, url)
        {
            Content = new StringContent(jsonPayload, System.Text.Encoding.UTF8, "application/json")
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
        Console.WriteLine($"   - 토큰 TTL: {_leaseDurationSeconds} 초, Renewable: {_isRenewable}");
    }

    // 2. 토큰 갱신 (POST /v1/auth/token/renew-self)
    private async Task<bool> ManualRenewTokenAsync(long remainingTtl)
    {
        if (!_isRenewable)
        {
            Console.WriteLine("⚠️ 토큰이 갱신 불가능합니다. 재인증이 필요합니다.");
            return false;
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
            return false;
        }

        using var document = JsonDocument.Parse(responseBody);
        var auth = document.RootElement.GetProperty("auth");

        var oldTtl = _leaseDurationSeconds;
        _leaseDurationSeconds = auth.GetProperty("lease_duration").GetInt64();
        _authTimeEpochSeconds = DateTimeOffset.UtcNow.ToUnixTimeSeconds();

        Console.WriteLine(">>> ✅ 토큰 갱신 성공!");
        Console.WriteLine($"    - 실행후 새로운 TTL: {_leaseDurationSeconds} 초 (이전 TTL: {oldTtl} 초)");
        return true;
    }

    // 3. KV Secret 조회 (GET /v1/{mount}/data/{path})
    public async Task ReadKvSecretAsync(string secretPath)
    {
        if (string.IsNullOrEmpty(_currentToken))
        {
            Console.WriteLine("🛑 Secret 조회 불가: 인증 토큰이 없습니다.");
            return;
        }
        
        var url = $"/v1/{_config.KvMountPath}/data/{secretPath}"; 
        
        // ⬅️ 수정: BaseAddress와 상대 URL을 조합하여 이중 슬래시를 방지하고 깔끔한 URL 로그 출력
        var fullUrl = new Uri(_httpClient.BaseAddress!, url).AbsoluteUri; 
        Console.WriteLine($">>> 🔎 KV Secret 요청 URL: {fullUrl}");

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
            secretData.Add(property.Name, property.Value.ToString() ?? string.Empty);
        }

        _secretsCache[secretPath] = secretData;
        
        var version = metadata.TryGetProperty("version", out var versionElement) 
                      ? versionElement.GetInt32().ToString() : "N/A";
                             
        Console.WriteLine($"   - ✅ Secret 조회/갱신 성공: {secretPath}, Version: {version}");
    }

    public void PrintSecretsCache()
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

    // 4. 스케줄러 로직
    public void StartScheduledTasks()
    {
        Console.WriteLine($"--- ♻️ KV Secrets 및 토큰 갱신 모니터링 스케쥴러 시작 (Interval: {_config.RenewalIntervalSeconds}초) ---");
        
        var intervalMs = _config.RenewalIntervalSeconds * 1000;
        
        var timer = new System.Threading.Timer(async _ =>
        {
            await ScheduledTaskAsync();
        }, null, intervalMs, intervalMs);
    }

    private async Task ScheduledTaskAsync()
    {
        // 1. 토큰 갱신 모니터링
        var remainingTtl = GetRemainingTtl();
        var renewalThreshold = (long)(_leaseDurationSeconds * (_config.TokenRenewalThresholdPercent / 100.0));
        Console.WriteLine($"\n⏳ [Token Manager] 토큰 잔여 TTL: {remainingTtl} 초 (갱신 임계점: {renewalThreshold} 초)");

        if (remainingTtl <= renewalThreshold && remainingTtl > 0)
        {
            try
            {
                var success = await ManualRenewTokenAsync(remainingTtl);
                if (!success) await AuthenticateAsync(); 
            }
            catch (Exception)
            {
                Console.WriteLine("❌ 토큰 갱신 오류. 재인증 시도...");
                await AuthenticateAsync();
            }
        }
        else if (remainingTtl <= 0)
        {
            Console.WriteLine("🛑 토큰 만료! 재인증을 시도합니다...");
            await AuthenticateAsync();
        }


        // 2. KV Secret 데이터 갱신 실행
        Console.WriteLine("\n--- ♻️ KV Secrets 갱신 스케줄러 실행 ---");
        foreach (var path in _config.KvSecretsPaths.Distinct())
        {
            await ReadKvSecretAsync(path);
        }
        PrintSecretsCache();
    }
}

// 프로그램 진입점
internal static class Program
{
    public static async Task Main(string[] args)
    {
        try
        {
            // 1. 설정 로드 (appsettings.json)
            var config = new ConfigurationBuilder()
                .SetBasePath(Directory.GetCurrentDirectory())
                .AddJsonFile("appsettings.json", optional: false, reloadOnChange: true)
                .Build();
                
            var vaultConfig = new VaultConfig();
            config.GetSection("Vault").Bind(vaultConfig);

            var client = new VaultClient(vaultConfig);

            // 2. 초기 인증 및 Secret 로드
            await client.AuthenticateAsync();
            
            Console.WriteLine("\n--- 🔎 초기 KV Secrets 조회 시작 ---");
            foreach (var path in vaultConfig.KvSecretsPaths.Distinct()) // ⬅️ 초기 로드 시 중복 방지
            {
                await client.ReadKvSecretAsync(path);
            }
            Console.WriteLine("✅ 초기 KV Secrets 조회 완료.");
            client.PrintSecretsCache();

            // 3. 스케줄러 시작
            client.StartScheduledTasks();
            
            Console.WriteLine("\n🚀 .NET Vault Client Started. Press Ctrl+C to exit.");
            await Task.Delay(System.Threading.Timeout.Infinite);
        }
        catch (Exception e)
        {
            Console.WriteLine($"❌ 애플리케이션 치명적 오류 발생: {e.Message}");
            Environment.Exit(1);
        }
    }
}