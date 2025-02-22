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
	virtual bool IsOpen() const;

	virtual bool EncodeFrame(const AVFrame* frame, AVPacket* packet) = 0;

	void SetBitRate(int bitrate);
	int BitRate() const;
	void SetCodecName(const std::string& codecName);
	std::string CodecName() const;

};

class AudioEncoder : public Encoder {
private:
	int sampleRate;
	int channels;
	int sampleFormat;
public:
	AudioEncoder();
	~AudioEncoder() override;

	bool Open() override;
	bool EncodeFrame(const AVFrame* frame, AVPacket* packet) override;

	// AudioEncoder-Specific methods
	void SetSampleRate(int sampleRate);
	int SampleRate() const;
	void SetChannels(int channels);
	int Channels() const;
	void SetSampleFormat(int sampleFormat);
	int SampleFormat() const;
};


class FrameEncoder : public Encoder {
private:
	int width;
	int height;
	int frameRate;
	int pixelFormat;
public:
	FrameEncoder();
	~FrameEncoder() override;

	bool Open() override;
	bool EncodeFrame(const AVFrame* frame, AVPacket* packet) override;

	// FrameEncoder-Specific methods
	void SetWidth(int width);
	int Width() const;
	void SetHeight(int height);
	int Height() const;
	void SetFrameRate(int frameRate);
	int FrameRate() const;
	void SetPixelFormat(int pixelFormat);
	int PixelFormat() const;
};