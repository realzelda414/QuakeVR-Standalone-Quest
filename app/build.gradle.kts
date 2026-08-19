plugins {
    id("com.android.application")
}

android {
    namespace = "com.vittorioromeo.quakevr.quest"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.vittorioromeo.quakevr.quest"
        minSdk = 29 // Android 10 (Meta Horizon OS baseline)
        targetSdk = 32
        versionCode = 1
        versionName = "1.0.0"

        ndk {
            abiFilters.add("arm64-v8a") // 64-bit ARM for Snapdragon XR2 Gen 2
        }

        externalNativeBuild {
            cmake {
                cppFlags("-std=c++20 -frtti -fexceptions")
                arguments("-DANDROID_STL=c++_shared")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            signingConfig = signingConfigs.getByName("debug") // Pre-signed for easy sideloading
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

dependencies {
    // Standard native dependencies
}
