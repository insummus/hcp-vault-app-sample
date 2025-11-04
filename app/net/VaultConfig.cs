using System.Collections.Generic;

namespace VaultClientDotnet;

public class VaultConfig
{
    public string Addr { get; set; } = string.Empty;
    public string Namespace { get; set; } = string.Empty;
    public string RoleId { get; set; } = string.Empty;
    public string SecretId { get; set; } = string.Empty;
    public string KvMountPath { get; set; } = "kv";
    public string[] KvSecretsPaths { get; set; } = { };
    public int RenewalIntervalSeconds { get; set; } = 10;
    public double TokenRenewalThresholdPercent { get; set; } = 20.0;
}