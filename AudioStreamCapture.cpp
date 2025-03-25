// AudioStreamCapture.cpp
#include "AudioStreamCapture.h"
#include <iostream>
#include <exception>
#include <string>
#include <vector>
#include <cmath>
#define REFTIMES_PER_SEC  100000 // 10Ms

AudioStreamCapture* AudioStreamCapture::instance = nullptr;
std::mutex AudioStreamCapture::mtx;
bool AudioStreamCapture::started_ = false;

AudioStreamCapture::~AudioStreamCapture() {
    if (m_captureClient) {
        m_captureClient.Reset();
    }
    if (m_audioClient) {
        m_audioClient.Reset();
    }
    if (m_device) {
        m_device.Reset();
    }
    if (m_deviceEnumerator) {
        m_deviceEnumerator.Reset();
    }
    if (m_waveFormat) {
        CoTaskMemFree(m_waveFormat);
    }
}

bool AudioStreamCapture::Initialize() {
    REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;

    // Get the default audio device
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&m_deviceEnumerator);
    if (FAILED(hr)) {
        std::cerr << "Failed to create MMDeviceEnumerator: " << std::hex << hr << std::endl;
        return false;
    }
    hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
    if (FAILED(hr)) {
        std::cerr << "Failed to get default audio endpoint: " << std::hex << hr << std::endl;
        return false;
    }

    // Activate the device
    hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_audioClient);
    if (FAILED(hr)) {
        std::cerr << "Failed to activate audio client: " << std::hex << hr << std::endl;
        return false;
    }

    // WebRTC works best with these specific parameters:
    // - 48kHz sample rate
    // - 10ms frames
    // - Stereo or Mono

   // First check if the format is supported
    WAVEFORMATEX webrtcFormat = { 0 };
    webrtcFormat.wFormatTag = WAVE_FORMAT_PCM;
    webrtcFormat.nChannels = 2;  // Mono
    webrtcFormat.nSamplesPerSec = 48000;  // 48kHz is the WebRTC internal standard
    webrtcFormat.wBitsPerSample = 16;  // 16-bit PCM
    webrtcFormat.nBlockAlign = webrtcFormat.nChannels * (webrtcFormat.wBitsPerSample / 8);
    webrtcFormat.nAvgBytesPerSec = webrtcFormat.nSamplesPerSec * webrtcFormat.nBlockAlign;

    // Check if format is supported
    WAVEFORMATEX* closestMatch = NULL;
    hr = m_audioClient->IsFormatSupported(
        AUDCLNT_SHAREMODE_SHARED,
        &webrtcFormat,
        &closestMatch);

    if (hr == S_OK) {
        
        hr = m_audioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK,
            hnsRequestedDuration,  // Buffer size
            0,                 // Period
            &webrtcFormat,     // Format
            NULL);             // Session GUID
    }
    else if (hr == S_FALSE && closestMatch) {
        // Your exact format isn't supported, but a similar one is
        std::cout << "Using closest matching format instead" << std::endl;

        // Initialize with the closest matching format
        hr = m_audioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK,
            hnsRequestedDuration,
            0,
            closestMatch,
            NULL);

        CoTaskMemFree(closestMatch);
    }
    else {
        // Format not supported and no alternative provided
        std::cerr << "Audio format not supported: " << std::hex << hr << std::endl;
        return false;
    }

    // Store the format for future use
    m_waveFormat = (WAVEFORMATEX*)CoTaskMemAlloc(sizeof(WAVEFORMATEX));
    if (!m_waveFormat) {
        std::cerr << "Failed to allocate memory for wave format." << std::endl;
        return false;
    }
    memcpy(m_waveFormat, &webrtcFormat, sizeof(WAVEFORMATEX));


    std::cout << "Audio client initialized with format: "
        << m_waveFormat->nChannels << " channels, "
        << m_waveFormat->nSamplesPerSec << " Hz, "
        << m_waveFormat->wBitsPerSample << " bits" << std::endl;
    // Get the capture client
    hr = m_audioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_captureClient);
    if (FAILED(hr)) {
        std::cerr << "Failed to get audio capture client: " << std::hex << hr << std::endl;
        return false;
    }

    return true;
}
void AudioStreamCapture::StartStream() {
    // Start the audio stream
    if (!m_audioClient) {
        std::cerr << "Audio client not initialized." << std::endl;
        return;
    }
    HRESULT hr = m_audioClient->Start();
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to start audio client: " + std::to_string(hr));
        return;
    }
    AudioStreamCapture::started_ = true;
}

void AudioStreamCapture::StopStream() {
    // Stop the audio stream
    if (!m_audioClient) {
        std::cerr << "Audio client not initialized." << std::endl;
        return;
    }
    HRESULT hr = m_audioClient->Stop();
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to stop audio client: " + std::to_string(hr));
        return;
    }
    AudioStreamCapture::started_ = false;
}

bool AudioStreamCapture::Started(){
    
    return AudioStreamCapture::started_;
}
void AudioStreamCapture::CaptureAudio(std::vector<BYTE>& data,
    int* bits_per_sample,
    int* sample_rate,
    size_t* number_of_channels,
    size_t* number_of_frames) {

    if (!m_captureClient || !m_waveFormat) {
        throw std::runtime_error("Audio capture client or wave format not initialized.");
    }
    if (!AudioStreamCapture::started_) {
        throw std::runtime_error("Audio stream not started.");
    }

    *bits_per_sample = m_waveFormat->wBitsPerSample;
    *number_of_channels = m_waveFormat->nChannels;
    *sample_rate = m_waveFormat->nSamplesPerSec;

    // WebRTC expects exactly 10ms frames
    UINT32 neededFrames = *sample_rate / 100;  // 480 frames at 48kHz
    UINT32 bytesPerFrame = (*bits_per_sample / 8) * (*number_of_channels);
    UINT32 totalBytesNeeded = neededFrames * bytesPerFrame;

    data.clear();
    data.reserve(totalBytesNeeded);

    BYTE* buffer;
    DWORD flags;
    UINT32 packetSize = 0;
    HRESULT hr = m_captureClient->GetNextPacketSize(&packetSize);

    if (FAILED(hr) || packetSize == 0) {
        *number_of_frames = 0;
        return;
    }

    UINT32 collectedFrames = 0;
    while (collectedFrames < neededFrames) {
        UINT32 framesAvailable = 0;
        hr = m_captureClient->GetBuffer(&buffer, &framesAvailable, &flags, nullptr, nullptr);
        if (FAILED(hr)) {
            *number_of_frames = 0;
            std::cerr << "Failed to get buffer: " << std::hex << hr << std::endl;
            return;
        }

        UINT32 framesToCopy = std::min(framesAvailable, neededFrames - collectedFrames);
        UINT32 bytesToCopy = framesToCopy * bytesPerFrame;

        data.insert(data.end(), buffer, buffer + bytesToCopy);
        collectedFrames += framesToCopy;

        hr = m_captureClient->ReleaseBuffer(framesAvailable);
        if (FAILED(hr)) {
            *number_of_frames = 0;
            std::cerr << "Failed to release buffer: " << std::hex << hr << std::endl;
            return;
        }
        hr = m_captureClient->GetNextPacketSize(&packetSize);
        if (FAILED(hr) || packetSize == 0) {
            break;
        }
    }

    *number_of_frames = collectedFrames;
}
