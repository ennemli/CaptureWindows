// Encoder.cpp
#include "Encoder.h"
extern "C" {
#include <libavcodec/avcodec.h>
}

Encoder::Encoder()
    : codecContext(nullptr), isOpen(false), bitrate(0) {
    // FFmpeg is automatically initialized via ffmpegInit member
}

Encoder::~Encoder() {
    Close();
    // FFmpeg is automatically uninitialized via ffmpegInit member
}

bool Encoder::Close() {
    if (codecContext) {
        avcodec_free_context(&codecContext);
        codecContext = nullptr;
        isOpen = false;
        return true;
    }
    return false;
}

bool Encoder::IsOpen() const {
    return isOpen;
}

void Encoder::SetBitrate(int newBitrate) {
    bitrate = newBitrate;
}

int Encoder::GetBitrate() const {
    return bitrate;
}

void Encoder::SetCodecName(const std::string& name) {
    codecName = name;
}

std::string Encoder::GetCodecName() const {
    return codecName;
}