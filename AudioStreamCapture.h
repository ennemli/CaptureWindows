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
    AudioStreamCapture() {

    }

    static std::mutex inst_mtx;
    static AudioStreamCapture* instance;
    static std::mutex started_mtx;
    static bool started_;
    static std::atomic<int> instance_count;

    bool Initialize();

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
            std::lock_guard<std::mutex> lock(AudioStreamCapture::inst_mtx);
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
	static bool Started();

    void CaptureAudio(std::vector<BYTE>& data,
        int* bits_per_sample,
        int* sample_rate,
        size_t* number_of_channels,
        size_t* number_of_frames);
    HRESULT  ReleaseBuffer(UINT32 number_of_frames) {
        if (!m_captureClient ||!Started()) {
            throw std::runtime_error("Capture Client is not initialized or audio stream has not started.");
        }
        return m_captureClient->ReleaseBuffer(number_of_frames);
    }

};
