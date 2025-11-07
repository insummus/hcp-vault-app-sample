plugins {
    kotlin("jvm") version "1.9.23"
    application
    id("com.google.devtools.ksp") version "1.9.23-1.0.20" 
}

group = "com.example"
version = "1.0-SNAPSHOT"

repositories {
    mavenCentral()
}

// Kotlin 컴파일러 설정
tasks.compileKotlin {
    kotlinOptions {
        jvmTarget = "17"
    }
}

// JAR 실행 설정
application {
    mainClass.set("com.example.vault.client.VaultClientKt")
}

dependencies {
    // Kotlin 기본 라이브러리
    implementation(kotlin("stdlib-jdk8"))

    // HTTP 클라이언트 (OkHttp)
    implementation("com.squareup.okhttp3:okhttp:4.12.0")

    // JSON 처리 (Moshi)
    implementation("com.squareup.moshi:moshi-kotlin:1.15.1")
    ksp("com.squareup.moshi:moshi-kotlin-codegen:1.15.1") 

    // 비동기 처리 (Kotlin Coroutines)
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.8.0")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-jdk8:1.8.0")

    // 설정 파일 로드 (Typesafe Config)
    implementation("com.typesafe:config:1.4.3")

    // Use the latest JUnit 5 library for testing.
    testImplementation("org.junit.jupiter:junit-jupiter-api:5.10.0")
    testRuntimeOnly("org.junit.jupiter:junit-jupiter-engine:5.10.0")
}