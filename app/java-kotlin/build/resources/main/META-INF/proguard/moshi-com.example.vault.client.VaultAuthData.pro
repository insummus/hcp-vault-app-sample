-if class com.example.vault.client.VaultAuthData
-keepnames class com.example.vault.client.VaultAuthData
-if class com.example.vault.client.VaultAuthData
-keep class com.example.vault.client.VaultAuthDataJsonAdapter {
    public <init>(com.squareup.moshi.Moshi);
}
