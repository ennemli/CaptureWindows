#pragma once
#include <Windows.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <Audioclient.h>
#include <mutex>
#include <atomic>
#include <iostream>
#include <memory>
#include "AudioData.h"
using Microsoft::WRL::ComPtr;

class AudioStreamCapture {
private:
    static std::mutex mtx;
    static AudioStreamCapture* instance;
    bool Initialize();
    AudioStreamCapture() {}

protected:
    ComPtr<IMMDeviceEnumerator> m_deviceEnumerator = nullptr;
    ComPtr<IMMDevice> m_device = nullptr;
    ComPtr<IAudioClient> m_audioClient = nullptr;
    ComPtr<IAudioCaptureClient> m_captureClient = nullptr;
    WAVEFORMATEX* m_waveFormat = nullptr;

public:
    ~AudioStreamCapture();
    AudioStreamCapture(const AudioStreamCapture&) = delete;
    AudioStreamCapture& operator=(const AudioStreamCapture&) = delete;

    static AudioStreamCapture* GetInstance() {
        if (!instance) {
            std::lock_guard<std::mutex> lock(mtx);
            if (!instance) {
                instance = new AudioStreamCapture();
                if (!instance->Initialize()) {
                    std::cerr << "Failed to initialize audio stream capture" << std::endl;
                    return nullptr;
                }
            }
        }
        return instance;
    }
    void StopStream();
    void StartStream();
    std::unique_ptr<AudioData> CaptureAudio();
};
