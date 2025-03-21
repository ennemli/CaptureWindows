//ScreenCapture.h
#pragma once
#include <optional>
#include <api/video/i420_buffer.h>
#include <api/video/video_frame.h>
#include <initguid.h>
#include <winsock2.h> 
#include <Windows.h>
#include <vector>
#include <ctype.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <mutex>
#include <iostream>

#include <third_party/libyuv/include/libyuv.h>
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

using Microsoft::WRL::ComPtr;

class ScreenCapture {
private:
	static std::mutex mtx;
	static ScreenCapture* instance;

	mutable std::atomic<int> ref_count{0};

	ComPtr<ID3D11Device> m_device = nullptr;
	ComPtr<ID3D11DeviceContext> m_context = nullptr;
	ComPtr<IDXGIOutputDuplication> m_duplication = nullptr;

	bool Initialize();
	ScreenCapture(){}

public:
	~ScreenCapture();
	ScreenCapture(const ScreenCapture&) = delete;
	ScreenCapture& operator=(const ScreenCapture&) = delete;
	static ScreenCapture* GetInstance() {
		if (!instance) {
			std::lock_guard<std::mutex> lock(mtx);
			if (!instance) {
				instance = new ScreenCapture();
				if (!instance->Initialize()) {
					std::cerr << "Failed to initialize screen capture" << std::endl;
					return nullptr;
				}
			}
		}
		return instance;
	}
	//void SaveToBitmap(const std::vector<uint8_t>& frameData, int w, int h, const wchar_t* filename);
	std::optional<webrtc::scoped_refptr<webrtc::I420Buffer>> CaptureFrame();
};