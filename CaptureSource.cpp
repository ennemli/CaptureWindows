// CaptureSource.h
#include "CaptureSource.h"

#include <api/video/i420_buffer.h>
#include <api/video/video_frame.h>
#include <third_party/libyuv/include/libyuv.h>
#include <rtc_base/timestamp_aligner.h>
#include <iostream>
#include <optional>

CaptureSource::~CaptureSource() {
	StopCapture();
}
void CaptureSource::StartCapture() {
	if (running_) return;
	running_ = true;
	capture_thread_->Start();
	capture_thread_->PostTask([this]() {
        webrtc::MutexLock lock(&mutex_);
		CaptureLoop();
		});
}
void CaptureSource::StopCapture() {
	running_ = false;
	if (capture_thread_) {
		capture_thread_->Stop();
		capture_thread_.reset();
	}
}

VideoCaptureSource::VideoCaptureSource() : m_screen_capture(ScreenCapture::GetInstance()),
stats_(new Stats){
    
    stats_->input_width = 1920;  // Default or actual values
    stats_->input_height = 1080; // Default or actual values

    StartCapture();
} 

void VideoCaptureSource::CaptureLoop() {
        if (!m_screen_capture) {
            std::cerr << "No Screen Capturer available" << std::endl;
            return;
        }
        int64_t last_capture_time_us_ = 0;
        int64_t  target_fps=33333; // 60 FPS
        const int kMaxConsecutiveFailures = 5;
        const int kInitialBackoffMs = 10;
        const int kMaxBackoffMs = 1000;
        int consecutive_failures_ = 0;
        int current_backoff_ms_ = kInitialBackoffMs;
        while (running_) {
			int64_t now_us = rtc::TimeMicros();
			int64_t elapsed_us = now_us - last_capture_time_us_;

            if (elapsed_us< target_fps) {
				rtc::Thread::Current()->SleepMs((target_fps - elapsed_us) / 1000);
            }
            std::optional<rtc::scoped_refptr<webrtc::I420Buffer>> 
                i420_buffer_opt = m_screen_capture->CaptureFrame();
             
            if (!i420_buffer_opt.has_value()) {
                consecutive_failures_++;
                if (consecutive_failures_ > kMaxConsecutiveFailures) {
                    rtc::Thread::Current()->SleepMs(current_backoff_ms_);
                    current_backoff_ms_ = std::min(current_backoff_ms_ * 2, kMaxBackoffMs);
                }
                continue;
            }
            else {
				consecutive_failures_ = 0;
				current_backoff_ms_ = kInitialBackoffMs;
            }
           stats_->input_width = i420_buffer_opt.value()->width();

                stats_->input_height = i420_buffer_opt.value()->height();

                webrtc::VideoFrame frame = webrtc::VideoFrame::Builder()
                    .set_video_frame_buffer(i420_buffer_opt.value())
                    .set_timestamp_us(rtc::TimeMicros())
                    .build();

                broadcaster_.OnFrame(frame);
                last_capture_time_us_ = now_us;

          
        }
}