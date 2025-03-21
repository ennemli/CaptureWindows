//ScreenCapture.cpp
#include "ScreenCapture.h"
#include <iostream>

using Microsoft::WRL::ComPtr;

ScreenCapture* ScreenCapture::instance = nullptr;
std::mutex ScreenCapture::mtx;

ScreenCapture::~ScreenCapture() {
    if (m_duplication) {
        m_duplication.Reset();
    }
    m_context.Reset();
    m_device.Reset();
}

bool ScreenCapture::Initialize() {
    D3D_FEATURE_LEVEL feature_level;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE,
        nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &m_device, &feature_level, &m_context);

    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D11 device: 0x" << std::hex << hr << std::endl;
        return false;
    }

    ComPtr<IDXGIDevice> dxgi_device;
    if (FAILED(m_device.As(&dxgi_device))) {
        std::cerr << "Failed to get DXGI device." << std::endl;
        return false;
    }

    ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgi_device->GetAdapter(&adapter))) {
        std::cerr << "Failed to get DXGI adapter." << std::endl;
        return false;
    }

    ComPtr<IDXGIOutput> output;
    if (FAILED(adapter->EnumOutputs(0, &output))) {
        std::cerr << "Failed to get DXGI output." << std::endl;
        return false;
    }

    ComPtr<IDXGIOutput1> output1;
    if (FAILED(output.As(&output1))) {
        std::cerr << "Failed to get DXGI output1." << std::endl;
        return false;
    }

    if (FAILED(output1->DuplicateOutput(m_device.Get(), &m_duplication))) {
        std::cerr << "Failed to duplicate output." << std::endl;
        return false;
    }
    return true;
}

std::optional<webrtc::scoped_refptr<webrtc::I420Buffer>> ScreenCapture::CaptureFrame() {
    if (!m_duplication) {
        std::cerr << "Duplication interface not initialized." << std::endl;
        return {};
    }

    DXGI_OUTDUPL_FRAME_INFO frame_info;
    ComPtr<IDXGIResource> resource;
    HRESULT hr = m_duplication->AcquireNextFrame(33, &frame_info, &resource);

    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            return {}; // Timeout, no error message needed for performance.
        }
        else if (hr == DXGI_ERROR_ACCESS_LOST) {
            std::cerr << "Access to desktop duplication was lost." << std::endl;
            return {};
        }
        else if (hr == E_ACCESSDENIED) {
            std::cerr << "Access denied." << std::endl;
            return {};
        }
        else {
            std::cerr << "Failed to acquire next frame: 0x" << std::hex << hr << std::endl;
            return {};
        }
    }

    ComPtr<ID3D11Texture2D> desktopImage;
    if (FAILED(resource.As(&desktopImage))) {
        std::cerr << "Failed to get texture." << std::endl;
        m_duplication->ReleaseFrame();
        return {};
    }

    D3D11_TEXTURE2D_DESC desc;
    desktopImage->GetDesc(&desc);
    int w = desc.Width;
    int h = desc.Height;

    D3D11_TEXTURE2D_DESC stagingDesc(desc);
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.BindFlags = 0;
    stagingDesc.MiscFlags = 0;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.SampleDesc.Count = 1;

    ComPtr<ID3D11Texture2D> stagingTexture;
    if (FAILED(m_device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture))) {
        std::cerr << "Failed to create staging texture." << std::endl;
        m_duplication->ReleaseFrame();
        return {};
    }

    m_context->CopyResource(stagingTexture.Get(), desktopImage.Get());

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    if (FAILED(m_context->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mappedResource))) {
        std::cerr << "Failed to map staging texture." << std::endl;
        m_duplication->ReleaseFrame();
        return {};
    }

    std::optional<webrtc::scoped_refptr<webrtc::I420Buffer>> i420_buffer_opt(webrtc::I420Buffer::Create(w, h));
    if (!i420_buffer_opt.has_value()) {
        std::cerr << "Failed to create I420 buffer." << std::endl;
        m_context->Unmap(stagingTexture.Get(), 0);
        m_duplication->ReleaseFrame();
        return {};
    }
    webrtc::scoped_refptr<webrtc::I420Buffer> i420_buffer = i420_buffer_opt.value();

    int conversion_result = libyuv::ARGBToI420(
        static_cast<uint8_t*>(mappedResource.pData),
        mappedResource.RowPitch,
        i420_buffer->MutableDataY(),
        i420_buffer->StrideY(),
        i420_buffer->MutableDataU(),
        i420_buffer->StrideU(),
        i420_buffer->MutableDataV(),
        i420_buffer->StrideV(),
        w,
        h
    );

    if (conversion_result != 0) {
        std::cerr << "Error converting ARGB to I420: " << conversion_result << std::endl;
        m_context->Unmap(stagingTexture.Get(), 0);
        m_duplication->ReleaseFrame();
        return {};
    }

    m_context->Unmap(stagingTexture.Get(), 0);
    m_duplication->ReleaseFrame();

    return i420_buffer_opt;
}