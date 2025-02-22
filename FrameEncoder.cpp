// FrameEncoder.cpp
#include "Encoder.h"
#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

FrameEncoder::FrameEncoder()
    : width(1280), height(720), frameRate(30), pixelFormat(AV_PIX_FMT_YUV420P) {
    // Default to H.264 codec
    SetCodecName("libx264");
    SetBitRate(2000000); // 2 Mbps default
}

FrameEncoder::~FrameEncoder() {
    Close();
}

bool FrameEncoder::Open() {
    // Already open?
    if (isOpen) return true;

    // Find the encoder
    const AVCodec* codec = avcodec_find_encoder_by_name(codecName.c_str());
    if (!codec) {
        std::cerr << "Could not find encoder for '" << codecName << "'" << std::endl;
        return false;
    }

    // Create codec context
    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        std::cerr << "Could not allocate video codec context" << std::endl;
        return false;
    }

    // Set parameters
    codecContext->bit_rate = bitrate;
    codecContext->width = width;
    codecContext->height = height;
    codecContext->time_base = av_make_q(1, frameRate);
    codecContext->framerate = av_make_q(frameRate,1);
    codecContext->gop_size = 10;
    codecContext->max_b_frames = 1;
    codecContext->pix_fmt = static_cast<AVPixelFormat>(pixelFormat);

    // Set codec-specific options
    if (codecName == "libx264") {
        av_opt_set(codecContext->priv_data, "preset", "medium", 0);
        av_opt_set(codecContext->priv_data, "tune", "zerolatency", 0);
    }

    // Open the codec
    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        std::cerr << "Could not open codec" << std::endl;
        avcodec_free_context(&codecContext);
        return false;
    }

    isOpen = true;
    return true;
}

bool FrameEncoder::EncodeFrame(const AVFrame* frame, AVPacket* packet) {
    if (!isOpen) {
        std::cerr << "Encoder not open" << std::endl;
        return false;
    }

    // Send the frame to the encoder
    int ret = avcodec_send_frame(codecContext, frame);
    if (ret < 0) {
        std::cerr << "Error sending frame for encoding" << std::endl;
        return false;
    }

    // Get encoded packets
    ret = avcodec_receive_packet(codecContext, packet);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return false; // Need more frames or end of stream
    }
    else if (ret < 0) {
        std::cerr << "Error during encoding" << std::endl;
        return false;
    }

    return true;
}

void FrameEncoder::SetWidth(int w) {
	width = w;
}

int FrameEncoder::Width() const {
	return width;
}

void FrameEncoder::SetHeight(int h) {
	height = h;
}

int FrameEncoder::Height() const {
	return height;
}

void FrameEncoder::SetFrameRate(int fps) {
    frameRate = fps;
}

int FrameEncoder::FrameRate() const {
    return frameRate;
}

void FrameEncoder::SetPixelFormat(int format) {
    pixelFormat = format;
}

int FrameEncoder::PixelFormat() const {
    return pixelFormat;
}