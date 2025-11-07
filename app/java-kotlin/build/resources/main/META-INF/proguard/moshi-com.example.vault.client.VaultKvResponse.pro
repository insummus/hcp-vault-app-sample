-if class com.example.vault.client.VaultKvResponse
-keepnames class com.example.vault.client.VaultKvResponse
-if class com.example.vault.client.VaultKvResponse
-keep class com.example.vault.client.VaultKvResponseJsonAdapter {
    public <init>(com.squareup.moshi.Moshi);
}
-if class com.example.vault.client.VaultKvResponse
-keepnames class kotlin.jvm.internal.DefaultConstructorMarker
-if class com.example.vault.client.VaultKvResponse
-keepclassmembers class com.example.vault.client.VaultKvResponse {
    public synthetic <init>(java.util.Map,int,kotlin.jvm.internal.DefaultConstructorMarker);
}
