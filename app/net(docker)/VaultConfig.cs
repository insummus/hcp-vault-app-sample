using System.Collections.Generic;
using System;

namespace NewVaultClientDotnet;

public class VaultConfig
{
    // Vault 접속 정보 (string.Empty로 초기화하여 설정 파일 로드를 강제)
    public string Addr { get; set; } = string.Empty;
    public string Namespace { get; set; } = string.Empty; 
    public string RoleId { get; set; } = string.Empty;
    public string SecretId { get; set; } = string.Empty;

    // KV Secret 설정
    public string KvMountPath { get; set; } = string.Empty;
    // 빈 배열로 초기화. KvSecretsPaths 항목이 없으면 빈 배열이 바인딩됩니다.
    public string[] KvSecretsPaths { get; set; } = Array.Empty<string>(); 
    
    // 스케줄링 설정 (0으로 초기화)
    public int RenewalIntervalSeconds { get; set; } = 0; 
    public double TokenRenewalThresholdPercent { get; set; } = 0.0; 
}