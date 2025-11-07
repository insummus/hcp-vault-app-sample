-if class com.example.vault.client.AuthPayload
-keepnames class com.example.vault.client.AuthPayload
-if class com.example.vault.client.AuthPayload
-keep class com.example.vault.client.AuthPayloadJsonAdapter {
    public <init>(com.squareup.moshi.Moshi);
}
