#include "FFmpegSystem.h"
#include <iostream>
extern "C" {
#include <libavformat/avformat.h>
}
std::atomic<int> FFmpegSystem::refCount(0);
std::mutex FFmpegSystem::initMutex;
bool FFmpegSystem::isInitialized = false;

bool FFmpegSystem::Initialize() {
	std::lock_guard<std::mutex> lock(initMutex);
	if (refCount.fetch_add(1) == 0) {
		std::cout << "Using FFmpeg version: "
			<< av_version_info() << std::endl;
		avformat_network_init();

		isInitialized = true;
		std::cout << "FFmpeg system initialized." << std::endl;

	}
	return isInitialized;
}

void FFmpegSystem::Uninitialize() {
	std::lock_guard<std::mutex> lock(initMutex);
	if (refCount.fetch_sub(-1)==1) {
		avformat_network_deinit();

		isInitialized = false;
		std::cout << "FFmpeg system uninitialized." << std::endl;
	}
}

FFmpegSystem& FFmpegSystem::GetInstance() {
	static FFmpegSystem instance;
	return instance;
}