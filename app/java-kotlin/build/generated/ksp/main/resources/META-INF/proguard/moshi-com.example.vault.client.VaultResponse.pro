-if class com.example.vault.client.VaultResponse
-keepnames class com.example.vault.client.VaultResponse
-if class com.example.vault.client.VaultResponse
-keep class com.example.vault.client.VaultResponseJsonAdapter {
    public <init>(com.squareup.moshi.Moshi);
}
-if class com.example.vault.client.VaultResponse
-keepnames class kotlin.jvm.internal.DefaultConstructorMarker
-if class com.example.vault.client.VaultResponse
-keepclassmembers class com.example.vault.client.VaultResponse {
    public synthetic <init>(com.example.vault.client.VaultAuthData,java.util.Map,java.util.List,int,kotlin.jvm.internal.DefaultConstructorMarker);
}
