////Audiiostreamcapture.h
//#include <Windows.h>
//#include <mmdeviceapi.h>
//#include <wrl/client.h>
//#include <Audioclient.h>
//#include <string>
//
//using Microsoft::WRL::ComPtr;
//class AudioStreamCapture {
//private:
//	ComPtr<IMMDeviceEnumerator> m_deviceEnumerator = nullptr;
//	ComPtr<IMMDevice> m_device = nullptr;
//	ComPtr<IAudioClient> m_audioClient = nullptr;
//	ComPtr<IAudioCaptureClient> m_captureClient = nullptr;
//	WAVEFORMATEX* m_waveFormat = nullptr;
//public:
//	bool Initialize();
//	void Start(const char* outputFileName);
//};