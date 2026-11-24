// droidHPSDR app: Kotlin + Jetpack Compose UI, native core (deskHPSDR + WDSP)
// and the GPU renderer from ../native as one JNI library.
// Needs ../../build-android/arm64-v8a from android/deps/build-deps.sh.
plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "io.github.randal007.droidhpsdr"
    compileSdk {
        version = release(37) { minorApiLevel = 2 }
    }
    ndkVersion = "27.3.13750724"

    defaultConfig {
        applicationId = "io.github.randal007.droidhpsdr"
        minSdk = 31
        targetSdk = 37
        versionCode = 1
        versionName = "0.3.0-m3"
        ndk { abiFilters += "arm64-v8a" }
        externalNativeBuild {
            cmake {
                targets += "droidhpsdr"
                arguments += listOf("-DANDROID_STL=none")
            }
        }
    }
    externalNativeBuild {
        cmake {
            path = file("../native/CMakeLists.txt")
            version = "4.1.2"
        }
    }
    buildTypes {
        release {
            isMinifyEnabled = false
            signingConfig = signingConfigs.getByName("debug")
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    buildFeatures { compose = true }
}

dependencies {
    val bom = platform("androidx.compose:compose-bom:2026.11.01")
    implementation(bom)
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.foundation:foundation")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.activity:activity-compose:1.13.0")
    implementation("androidx.core:core-ktx:1.19.1")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.11.1")
}
