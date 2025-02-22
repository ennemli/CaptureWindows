//ScreenCapture.h
#pragma once
#include <vector>
#include <ctype.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
using Microsoft::WRL::ComPtr;
class ScreenCapture {
private:
	ComPtr<ID3D11Device> m_device=nullptr;
	ComPtr<ID3D11DeviceContext> m_context=nullptr;
	ComPtr<IDXGIOutputDuplication> m_duplication=nullptr;

public:
	bool Initialize();
	bool CaptureFrame(std::vector<uint8_t>& frameData, int& w, int& h);
	void SaveToBitmap(const std::vector<uint8_t>& frameData, int w, int h, const wchar_t* filename);
}; 
