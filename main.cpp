//Main.cpp
#include "ScreenCapture.h"
#include <vector>
#include <cstdint>
#include <iostream>
#include <combaseapi.h>
#include <string>
#include "AudioStreamCapture.h"
int main() {
	CoInitialize(nullptr);
	{

		AudioStreamCapture asc;
		if (!asc.Initialize()) {
			return 1;
		}
		asc.Start("audio.mp3");
		/*ScreenCapture sc;
		if (!sc.Initialize()) {
			return 1;
		}*/
		//int f = 0;

		//while ((++f)<=25) {
			//std::vector<uint8_t> frameData;

			//int w, h;

			/*if (!sc.CaptureFrame(frameData, w, h)) {
				return 1;
			}*/
			//std::wstring filename = L"screenshot_" + std::to_wstring(f) + L"_.webp";
			//sc.SaveToBitmap(frameData, w, h, filename.c_str());
		//}
	}
	CoUninitialize();
	return 0;
}