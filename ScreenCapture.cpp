#include "ScreenCapture.h"
#include <iostream>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

bool ScreenCapture::Initialize() {
	// Create device and context
	D3D_FEATURE_LEVEL feature_level;
	HRESULT hr = D3D11CreateDevice
	(nullptr, D3D_DRIVER_TYPE_HARDWARE,
		nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &m_device, &feature_level, &m_context);
	if (FAILED(hr)) {
		std::cerr << "Failed to create D3D11 device: " << std::hex << hr << std::endl;
		return false;
	}
	ComPtr<IDXGIDevice> dxgi_device;
	hr = m_device.As(&dxgi_device);
	if (FAILED(hr)) {
		std::cerr << "Failed to get DXGI device: " << std::hex << hr << std::endl;
		return false;
	}

	ComPtr<IDXGIAdapter> adapter;
	hr = dxgi_device->GetAdapter(&adapter);
	if (FAILED(hr)) {
		std::cerr << "Failed to get DXGI adapter: " << std::hex << hr << std::endl;
		return false;
	}

	ComPtr<IDXGIOutput> output;
	hr = adapter->EnumOutputs(0, &output);
	if (FAILED(hr)) {
		std::cerr << "Failed to get DXGI output: " << std::hex << hr << std::endl;
		return false;
	}

	ComPtr<IDXGIOutput1> output1;
	hr = output.As(&output1);
	if (FAILED(hr)) {
		std::cerr << "Failed to get DXGI output1: " << std::hex << hr << std::endl;
		return false;
	}

	hr = output1->DuplicateOutput(m_device.Get(), &m_duplication);
	if (FAILED(hr)) {
		std::cerr << "Failed to duplicate output: " << std::hex << hr << std::endl;
		return false;
	}
	return true;
}

bool ScreenCapture::CaptureFrame(std::vector<uint8_t>& frameData,int& w,int& h){
	if (!m_duplication) {
		return false;
	}
	DXGI_OUTDUPL_FRAME_INFO frame_info;
	ComPtr<IDXGIResource> resource;
	HRESULT hr = m_duplication->AcquireNextFrame(500, &frame_info, &resource);
	
	// Get a frame
	if (FAILED(hr)) {
		if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
			return false; // No frame available
		}
		std::cerr << "Failed to acquire next frame: " << std::hex << hr << std::endl;
		return false;
	}

	// Get the desktop image
	ComPtr<ID3D11Texture2D> desktopImage;
	hr = resource.As(&desktopImage);
	if (FAILED(hr)) {
		std::cerr << "Failed to get texture: " << std::hex << hr << std::endl;
		m_duplication->ReleaseFrame();
		return false;
	}

	// Get texture description
	D3D11_TEXTURE2D_DESC desc;
	desktopImage->GetDesc(&desc);
	w = desc.Width;
	h = desc.Height;

	// Create a staging texture for CPU access
	D3D11_TEXTURE2D_DESC stagingDesc =desc;
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingDesc.BindFlags = 0;
	stagingDesc.MiscFlags = 0;
	stagingDesc.MipLevels = 1;
	stagingDesc.ArraySize = 1;
	stagingDesc.SampleDesc.Count = 1;

	// Create the staging texture
	ComPtr<ID3D11Texture2D> stagingTexture;
	hr = m_device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
	if (FAILED(hr)) {
		std::cerr << "Failed to create staging texture: " << std::hex << hr << std::endl;
		m_duplication->ReleaseFrame();
		return false;
	}
	m_context->CopyResource(stagingTexture.Get(), desktopImage.Get());

	// Map the staging texture
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	hr = m_context->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
	if (FAILED(hr)) {
		std::cerr << "Failed to map staging texture: " << std::hex << hr << std::endl;
		m_duplication->ReleaseFrame();
		return false;
	}

	// Copy the image data (CORRECTED)
	uint8_t* source = static_cast<uint8_t*>(mappedResource.pData);
	frameData.resize(h * w * 4); // Resize using actual width

	for (int row = 0; row < h; row++) {
		memcpy(
			frameData.data() + (row * w * 4), // Use w * 4 for BGRA data
			source + (row * mappedResource.RowPitch),
			w * 4 
		);
	}

	// Unmap and release
	m_context->Unmap(stagingTexture.Get(), 0);
	m_duplication->ReleaseFrame();

	return true;
}


void ScreenCapture::SaveToBitmap(const std::vector<uint8_t>& frameData, int w, int h, const wchar_t* filename) {
	// Initialize COM
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (FAILED(hr)) {
		std::cerr << "Failed to initialize COM." << std::endl;
		return;
	}

	// Create WIC factory
	ComPtr<IWICImagingFactory> wicFactory;
	hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
	if (FAILED(hr)) {
		std::cerr << "Failed to create WIC Imaging Factory." << std::endl;
		CoUninitialize();
		return;
	}

	// Create a WIC Bitmap from memory
	ComPtr<IWICBitmap> bitMap;
	hr = wicFactory->CreateBitmapFromMemory(
		w, h, GUID_WICPixelFormat32bppBGRA, w * 4, static_cast<UINT>(frameData.size()),
		const_cast<BYTE*>(frameData.data()), &bitMap);
	if (FAILED(hr)) {
		std::cerr << "Failed to create WIC Bitmap from memory." << std::endl;
		CoUninitialize();
		return;
	}

	ComPtr<IWICStream> stream;
	ComPtr<IWICBitmapEncoder> encoder;

	if (FAILED(wicFactory->CreateStream(&stream)) ||
		FAILED(stream->InitializeFromFilename(filename, GENERIC_WRITE)) ||
		FAILED(wicFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) ||
		FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache))) {
		std::cerr << "Failed to initialize WIC encoder\n";
		CoUninitialize();

		return ;
	}

	// Create a frame
	ComPtr<IWICBitmapFrameEncode> frame;
	ComPtr<IPropertyBag2> props;
	hr = encoder->CreateNewFrame(&frame, &props);
	if (FAILED(hr)) {
		std::cerr << "Failed to create WebP frame." << std::endl;
		CoUninitialize();
		return;
	}

	hr = frame->Initialize(props.Get());
	if (FAILED(hr)) {
		std::cerr << "Failed to initialize WebP frame." << std::endl;
		CoUninitialize();
		return;
	}

	hr = frame->SetSize(w, h);
	if (FAILED(hr)) {
		std::cerr << "Failed to set WebP frame size." << std::endl;
		CoUninitialize();
		return;
	}

	WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
	hr = frame->SetPixelFormat(&format);
	if (FAILED(hr)) {
		std::cerr << "Failed to set WebP pixel format." << std::endl;
		CoUninitialize();
		return;
	}

	// Write pixel data
	hr = frame->WriteSource(bitMap.Get(), nullptr);
	if (FAILED(hr)) {
		std::cerr << "Failed to write WebP image data." << std::endl;
		CoUninitialize();
		return;
	}

	// Commit the frame
	hr = frame->Commit();
	if (FAILED(hr)) {
		std::cerr << "Failed to commit WebP frame." << std::endl;
		CoUninitialize();
		return;
	}

	// Commit the encoder
	hr = encoder->Commit();
	if (FAILED(hr)) {
		std::cerr << "Failed to commit WebP encoder." << std::endl;
		CoUninitialize();
		return;
	}

	// Cleanup (handled by ComPtr)
	CoUninitialize();

	std::wcout << L"WebP image saved to " << filename << std::endl;
}
