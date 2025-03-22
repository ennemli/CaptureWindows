//AudioStreamCapture.cpp
#include "AudioStreamCapture.h"
#include <iostream>
#include <exception>
#include <string>

AudioStreamCapture* AudioStreamCapture::instance = nullptr;
std::mutex AudioStreamCapture::mtx;

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
	// Get the default audio device
	HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),(void**)&m_deviceEnumerator);
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

	// Get Format of the audio stream
	hr = m_audioClient->GetMixFormat(&m_waveFormat);
	if (FAILED(hr)) {
		std::cerr << "Failed to get mix format: " << std::hex << hr << std::endl;
		return false;
	}

	//Initialize the audio stream
	hr = m_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 10000000, 0, m_waveFormat, nullptr);
	if (FAILED(hr)) {
		std::cerr << "Failed to initialize audio client: " << std::hex << hr << std::endl;
		return false;
	}
	hr = m_audioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_captureClient);
	if (FAILED(hr)) {
		std::cerr << "Failed to get audio capture client: " << std::hex << hr << std::endl;
		return false;
	}

	return true;
}

void AudioStreamCapture::StartStream() {
	// Start the audio stream
	HRESULT hr = m_audioClient->Start();
	if (FAILED(hr)) {
		throw std::runtime_error("Failed to start audio client: " + std::to_string(hr));
		return;
	}
}

void AudioStreamCapture::StopStream() {
	// Stop the audio stream
	HRESULT hr = m_audioClient->Stop();
	if (FAILED(hr)) {
		throw std::runtime_error("Failed to stop audio client: " + std::to_string(hr));
		return;
	}
}

std::unique_ptr<AudioData> AudioStreamCapture::CaptureAudio() {
	// Get the next packet size
	UINT32 framesAvailable;
	DWORD flags;
	HRESULT hr = m_captureClient->GetNextPacketSize(&framesAvailable);
	if (FAILED(hr)) {
		std::cerr << "Failed to get next packet size: " << std::hex << hr << std::endl;
		return nullptr;
	}
	// Get the audio data
	BYTE* data;
	UINT32 numFramesAvailable;
	hr = m_captureClient->GetBuffer(&data, &numFramesAvailable, &flags, nullptr, nullptr);
	if (FAILED(hr)) {
		std::cerr << "Failed to get buffer: " << std::hex << hr << std::endl;
		return nullptr;
	}
	// Create the audio data object
	std::unique_ptr<AudioData> audioData = std::make_unique< AudioData>();
	audioData->bits_per_sample = m_waveFormat->wBitsPerSample;
	audioData->sample_rate = m_waveFormat->nSamplesPerSec;
	audioData->number_of_channels = m_waveFormat->nChannels;
	audioData->number_of_frames = numFramesAvailable;
	audioData->audio_data = data;
	// Release the buffer
	hr = m_captureClient->ReleaseBuffer(numFramesAvailable);
	if (FAILED(hr)) {
		std::cerr << "Failed to release buffer: " << std::hex << hr << std::endl;
		return nullptr;
	}
	return audioData;
}

//void AudioStreamCapture::Start() {
//	// Start the audio stream
//	HRESULT hr = m_audioClient->Start();
//	if (FAILED(hr)) {
//		std::cerr << "Failed to start audio client: " << std::hex << hr << std::endl;
//		return;
//	}
//	// Open the output file
//	FILE* outputFile;
//	fopen_s(&outputFile, outputFileName, "wb");
//	if (!outputFile) {
//		std::cerr << "Failed to open output file." << std::endl;
//		return;
//	}
//	// Capture audio data
//	BYTE* data;
//	UINT32 framesAvailable;
//	DWORD flags;
//	while (true) {
//		hr = m_captureClient->GetNextPacketSize(&framesAvailable);
//		if (FAILED(hr)) {
//			std::cerr << "Failed to get next packet size: " << std::hex << hr << std::endl;
//			break;
//		}
//		while (framesAvailable > 0) {
//			UINT32 numFramesAvailable;
//			hr = m_captureClient->GetBuffer(&data, &numFramesAvailable, &flags, nullptr, nullptr);
//			if (SUCCEEDED(hr)) {
//				size_t bytesToWrite = numFramesAvailable * m_waveFormat->nBlockAlign;
//				fwrite(data, 1, bytesToWrite, outputFile);
//				m_captureClient->ReleaseBuffer(numFramesAvailable);
//
//			}
//			hr = m_captureClient->GetNextPacketSize(&framesAvailable);
//
//		}
//		Sleep(10);
//	}
//	// Cleanup
//	m_audioClient->Stop();
//	m_captureClient->Release();
//	m_audioClient->Release();
//	m_device->Release();
//	m_deviceEnumerator->Release();
//}