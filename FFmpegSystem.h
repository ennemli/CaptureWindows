// FFmpegSystem.h
#pragma once
#include <mutex>
#include <atomic>

// Class to handle global FFmpeg initialization and cleanup
class FFmpegSystem {
private:
    static std::atomic<int> refCount;
    static std::mutex initMutex;
    static bool isInitialized;

    // Private constructor prevents direct instantiation
    FFmpegSystem() = default;
	FFmpegSystem& operator=(const FFmpegSystem&) = delete;
	FFmpegSystem(const FFmpegSystem&) = delete;


public:
    // Initialize FFmpeg libraries
    static bool Initialize();

    // Uninitialize FFmpeg libraries
    static void Uninitialize();

    // Get singleton instance
    static FFmpegSystem& GetInstance();
};

// Helper class for automatic initialization/uninitialization
class FFmpegInitializer {
public:
    FFmpegInitializer() {
        FFmpegSystem::Initialize();
    }

    ~FFmpegInitializer() {
        FFmpegSystem::Uninitialize();
    }
};