#pragma once
#include "FFmpegSystem.h"
#include <string>
#include <memory>

struct AVCodecContext;
struct AVFrame;
struct AVPacket;

class Encoder {
protected:
	FFmpegInitializer ffmpegInit;

	AVCodecContext* codecContext;
	bool isOpen;
	std::string codecName;
	int bitrate;
public:
	Encoder();
	virtual ~Encoder();

	virtual bool Open() = 0;
	virtual void Close();
	virtual bool isOpen() const;

	virtual bool EncodeFrame(const AVFrame* frame, AVPacket* packet) = 0;

	void SetBitRate(int bitrate);
	int BitRate() const;
	void SetCodecName(const std::string& codecName);
	std::string CodecName() const;

};