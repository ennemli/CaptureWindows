#pragma once
#include "FFmpegSystem.h"
#include <string>
#include <memory>

struct AVCodecContext;
struct AVFrame;
struct AVPacket;

class Enocder {
protected:
	FFmpegInitializer ffmpegInit;

	AVCodecContext* codecContext;
	bool isOpen;
	std::string codecName;
	int bitrate;
public:
	Enocder();
	virtual ~Enocder();

	virtual bool Open() = 0;
	virtual void Close();
	virtual bool isOpen() const;

	virtual bool EncodeFrame(const AVFrame* frame, AVPacket* packet) = 0;

	void setBitRate(int bitrate);
	int getBitRate() const;
	void setCodecName(const std::string& codecName);
	std::string getCodecName() const;

};
