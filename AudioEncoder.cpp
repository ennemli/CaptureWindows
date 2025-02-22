//AudioEncoder.cpp
#include "Encoder.h"
#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/error.h>
}

AudioEncoder::AudioEncoder()
	: sampleRate(44100), channels(2), sampleFormat(AV_SAMPLE_FMT_FLTP) {
	SetCodecName("opus");
}

AudioEncoder::~AudioEncoder() {
	Close();
}

bool AudioEncoder::Open() {
	if (isOpen) return true; // Check if already open
	const AVCodec* codec = avcodec_find_encoder_by_name(CodecName().c_str());
	if (!codec) {
		std::cerr << "Failed to find codec: " << CodecName() << std::endl;
		return false;
	}
	codecContext = avcodec_alloc_context3(codec);
	if (!codecContext) {
		std::cerr << "Failed to allocate codec context." << std::endl;
		return false;
	}
	codecContext->bit_rate = bitrate;
	codecContext->sample_rate = sampleRate;
	codecContext->sample_fmt = static_cast<AVSampleFormat>(sampleFormat);
	av_channel_layout_default(&codecContext->ch_layout, channels);

	if (int ret = avcodec_open2(codecContext, codec, nullptr); ret < 0) {
		std::cerr << "Failed to open codec." << std::endl;
		return false;
	}
	isOpen = true;
	return true;
}

bool AudioEncoder::EncodeFrame(const AVFrame* frame, AVPacket* packet) {
	if (!isOpen) {
		std::cerr << "Encoder is not open." << std::endl;
		return false;
	}
	int ret = avcodec_send_frame(codecContext, frame);
	if (ret < 0) {
		std::cerr << "Failed to send frame." << std::endl;
		return false;
	}
	ret = avcodec_receive_packet(codecContext, packet);
	if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
		return false; // Need more frames or end of stream
	} else if (ret < 0) {
		std::cerr << "Error during encoding" << std::endl;
		return false;
	}
	return true;
}

void AudioEncoder::SetSampleRate(int sampleRate) {
	this->sampleRate = sampleRate;
}

int AudioEncoder::SampleRate() const {
	return sampleRate;
}

void AudioEncoder::SetChannels(int channels) {
	this->channels = channels;
}

int AudioEncoder::Channels() const {
	return channels;
}

void AudioEncoder::SetSampleFormat(int sampleFormat) {
	this->sampleFormat = sampleFormat;
}

int AudioEncoder::SampleFormat() const {
	return sampleFormat;
}