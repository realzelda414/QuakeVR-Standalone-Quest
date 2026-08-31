plugins {
    id("com.android.application")
}

android {
    namespace = "com.vittorioromeo.quakevr.quest"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.vittorioromeo.quakevr.quest"
        minSdk = 29
        targetSdk = 34
        versionCode = 1
        versionName = "1.0.0"

        ndk {
            abiFilters.addAll(listOf("arm64-v8a"))
        }

        externalNativeBuild {
            cmake {
                cppFlags("-std=c++20 -frtti -fexceptions")
                // Enable verbose CMake / Ninja output to aid debugging native build failures
                arguments("-DANDROID_STL=c++_shared", "-DCMAKE_VERBOSE_MAKEFILE=ON")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            signingConfig = signingConfigs.getByName("debug")
        }
    }

    // Disable Vulkan glslc shader compilation for OpenGL ES GLSL shaders
    sourceSets {
        getByName("main") {
            shaders.setSrcDirs(emptyList<String>())
            assets.srcDirs("src/main/assets", "src/main/shaders")
        }
    }

    lint {
        checkReleaseBuilds = false
        abortOnError = false
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
    // Native OpenXR application with no Java UI dependencies required
}
