#pragma once
#include <media/base/video_broadcaster.h>
#include <api/peer_connection_interface.h>
#include <api/media_stream_interface.h>
#include <absl/types/optional.h>
#include "ScreenCapture.h"
#include "AudioStreamCapture.h"
#include <memory>
#include <iostream>
#include <rtc_base/synchronization/mutex.h>
#include <unordered_map>
class CaptureSource {
public:
    ~CaptureSource();
    virtual void StartCapture();
    virtual void StopCapture();
protected:
    virtual void CaptureLoop() = 0;
    std::unique_ptr<rtc::Thread> capture_thread_ = rtc::Thread::Thread::Create();
    std::atomic<bool> running_ = false;
    webrtc::Mutex mutex_;

};

// VideoTrackSourceInterface is a reference counted source used for
// VideoTracks. The same source can be used by multiple VideoTracks.
// VideoTrackSourceInterface is designed to be invoked on the signaling thread
// except for rtc::VideoSourceInterface<VideoFrame> methods that will be invoked
// on the worker thread via a VideoTrack. A custom implementation of a source
// can inherit AdaptedVideoTrackSource instead of directly implementing this
// interface.
class VideoCaptureSource :public CaptureSource,public webrtc::VideoTrackSourceInterface {
public:
    VideoCaptureSource();
    ~VideoCaptureSource();

    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
        const rtc::VideoSinkWants& wants) override {
        broadcaster_.AddOrUpdateSink(sink, wants);
    }

    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override {
        broadcaster_.RemoveSink(sink);
    }
    // Indicates that parameters suitable for screencasts should be automatically
    // applied to RtpSenders.
    // TODO(perkj): Remove these once all known applications have moved to
    // explicitly setting suitable parameters for screencasts and don't need this
    // implicit behavior.
    virtual bool is_screencast() const override { return true; };

    // Indicates that the encoder should denoise video before encoding it.
    // If it is not set, the default configuration is used which is different
    // depending on video codec.
    // TODO(perkj): Remove this once denoising is done by the source, and not by
    // the encoder.
    std::optional<bool> needs_denoising() const override { return false; };

    // Returns false if no stats are available, e.g, for a remote source, or a
    // source which has not seen its first frame yet.
    //
    // Implementation should avoid blocking.
    bool GetStats(Stats* stats) override {
        stats->input_width = stats_->input_width;
        stats->input_height = stats_->input_height;
        return true;
    };

    // Returns true if encoded output can be enabled in the source.
    bool SupportsEncodedOutput() const override { return false; };

    // Reliably cause a key frame to be generated in encoded output.
    // TODO(bugs.webrtc.org/11115): find optimal naming.
    void GenerateKeyFrame() override {};

    // Add an encoded video sink to the source and additionally cause
    // a key frame to be generated from the source. The sink will be
    // invoked from a decoder queue.
    void AddEncodedSink(
        rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink) override {
        std::cout << "EncodedSink" << "\n";
    };

    // Removes an encoded video sink from the source.
     void RemoveEncodedSink(
        rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink) override {

    };

    // Notify about constraints set on the source. The information eventually gets
    // routed to attached sinks via VideoSinkInterface<>::OnConstraintsChanged.
    // The call is expected to happen on the network thread.
    // TODO(crbug/1255737): make pure virtual once downstream project adapts.
    void ProcessConstraints(
        const webrtc::VideoTrackSourceConstraints& /* constraints */) override {
    }
    // Live or ended. A track will never be live again after becoming ended.
    webrtc::MediaSourceInterface::SourceState state() const override {
        return webrtc::MediaSourceInterface::SourceState::kLive;
    }

    // Indicates if the source is remote.
    bool remote() const override {
        return false;
    }

    void RegisterObserver(webrtc::ObserverInterface* observer) override {
        // Implement observer registration
    }

    void UnregisterObserver(webrtc::ObserverInterface* observer) override {
        // Implement observer unregistration
    }

    void AddRef() const override { ++ref_count_; }

    webrtc::RefCountReleaseStatus Release()const override {
        int count = --ref_count_;
        if (count == 0) {
            delete this;
            return webrtc::RefCountReleaseStatus::kDroppedLastRef;
        }
        return webrtc::RefCountReleaseStatus::kOtherRefsRemained;
    }
protected:
    void CaptureLoop() override;
private:
    rtc::VideoBroadcaster broadcaster_;
    mutable std::atomic<int> ref_count_ = 0;
    ScreenCapture* m_screen_capture;

    Stats* stats_;


};



class VideoCaptureTrack : public webrtc::VideoTrackInterface{
public:
    VideoCaptureTrack(std::string track_id) :enabled_(true),track_id_(std::move(track_id)),
        video_track_source(new VideoCaptureSource){
        video_track_source->AddRef();
        
    };
    ~VideoCaptureTrack() noexcept override {
		video_track_source->Release();
    };

    // VideoTrackSourceInterface implementation
    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
        const rtc::VideoSinkWants& wants) override {
        video_track_source->AddOrUpdateSink(sink, wants);
    }

    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override {
        video_track_source->RemoveSink(sink);
    }
    
    ContentHint content_hint() const override {
        return ContentHint::kNone;
    }
    
    void set_content_hint(ContentHint /* hint */) override {};

    std::string kind() const override {
        return kVideoKind;
    }

    // Track identifier.
    std::string id() const override { return track_id_; }

    // A disabled track will produce silence (if audio) or black frames (if
    // video). Can be disabled and re-enabled.
    bool enabled() const override { return enabled_; }
    bool set_enabled(bool enable) override{
        enabled_ = enable;
        return enabled_;
    }

    // Live or ended. A track will never be live again after becoming ended.
    TrackState state() const override {
        return kLive;
    };

    webrtc::VideoTrackSourceInterface* GetSource() const override {        
        return video_track_source; 
    }


    void RegisterObserver(webrtc::ObserverInterface* observer) override {
        
    }

    void UnregisterObserver(webrtc::ObserverInterface* observer) override {
    }

    void AddRef() const override{ ++ref_count_; }
    
    webrtc::RefCountReleaseStatus Release()const override{
        int count = --ref_count_;
        if (count == 0) {
            delete this;
            return webrtc::RefCountReleaseStatus::kDroppedLastRef;
        }
        return webrtc::RefCountReleaseStatus::kOtherRefsRemained;
    }


private:
    mutable std::atomic<int> ref_count_ = 0;
    bool enabled_;

    std::string track_id_;
    VideoCaptureSource* video_track_source;
};


class AudioBroadcaster : public webrtc::AudioTrackSinkInterface,public webrtc::RefCountInterface {
public:
    void AddSink(webrtc::AudioTrackSinkInterface* sink)  {
        webrtc::MutexLock lock(&mutex_);
        sinks_.insert({ sink,sink });
    }

    void RemoveSink(webrtc::AudioTrackSinkInterface* sink)  {
        webrtc::MutexLock lock(&mutex_);
        sinks_.erase(sink);
    }

    void OnData(const void* audio_data,
        int bits_per_sample,
        int sample_rate,
        size_t number_of_channels,
        size_t number_of_frames,
        std::optional<int64_t> timestamp) override{

        webrtc::MutexLock lock(&mutex_);
        for (const auto& [_, sink] : sinks_) {
            sink->OnData(audio_data, bits_per_sample, 
                sample_rate, number_of_channels, number_of_frames, timestamp);
        }
    }
    void AddRef() const override { ++ref_count_; }

    webrtc::RefCountReleaseStatus Release()const override {
        int count = --ref_count_;
        if (count == 0) {
            delete this;
            return webrtc::RefCountReleaseStatus::kDroppedLastRef;
        }
        return webrtc::RefCountReleaseStatus::kOtherRefsRemained;
    }

private:
    webrtc::Mutex mutex_;
    std::unordered_map<webrtc::AudioTrackSinkInterface*, 
       webrtc::AudioTrackSinkInterface*> sinks_;
    mutable std::atomic<int> ref_count_ = 0;

};


// AudioSourceInterface is a reference counted source used for AudioTracks.
// The same source can be used by multiple AudioTracks.
class  AudioCaptureSource :public CaptureSource, public webrtc::AudioSourceInterface {
public:
    // TODO(deadbeef): Makes all the interfaces pure virtual after they're
    // implemented in chromium.

    // Sets the volume of the source. `volume` is in  the range of [0, 10].
    // TODO(tommi): This method should be on the track and ideally volume should
    // be applied in the track in a way that does not affect clones of the track.
    AudioCaptureSource();
    ~AudioCaptureSource();
    void SetVolume(double /* volume */) override {}

    // Registers/unregisters observers to the audio source.
    void RegisterAudioObserver(AudioObserver* /* observer */) override {}
    void UnregisterAudioObserver(AudioObserver* /* observer */) override {}

    // TODO(tommi): Make pure virtual.
    void AddSink(webrtc::AudioTrackSinkInterface*  sink ) override{
		m_audio_broadcaster->AddSink(sink);
    }
    void RemoveSink(webrtc::AudioTrackSinkInterface*  sink ) override {
		m_audio_broadcaster->RemoveSink(sink);
    }

    // Returns options for the AudioSource.
    // (for some of the settings this approach is broken, e.g. setting
    // audio network adaptation on the source is the wrong layer of abstraction).
    //const cricket::AudioOptions options() const  {};
	
     // Live or ended. A track will never be live again after becoming ended.
    webrtc::MediaSourceInterface::SourceState state() const override {
        return webrtc::MediaSourceInterface::SourceState::kLive;
    }

    // Indicates if the source is remote.
    bool remote() const override {
        return false;
    }

    void RegisterObserver(webrtc::ObserverInterface* observer) override {
        // Implement observer registration
    }

    void UnregisterObserver(webrtc::ObserverInterface* observer) override {
        // Implement observer unregistration
    }

    void AddRef() const override { ++ref_count_; }

    webrtc::RefCountReleaseStatus Release()const override {
        int count = --ref_count_;
        if (count == 0) {
            delete this;
            return webrtc::RefCountReleaseStatus::kDroppedLastRef;
        }
        return webrtc::RefCountReleaseStatus::kOtherRefsRemained;
    }
    
    
    void StartCapture() override;
	void StopCapture() override;

protected:
	void CaptureLoop() override;
private:

    webrtc::scoped_refptr<AudioBroadcaster> m_audio_broadcaster;
	AudioStreamCapture* m_audio_stream_capture;
    mutable std::atomic<int> ref_count_ = 0;

};


class AudioCaptureTrack : public webrtc::AudioTrackInterface {
public:
    AudioCaptureTrack(std::string track_id):
        enabled_(true), track_id_(std::move(track_id)), 
        m_audio_source(new AudioCaptureSource){
		m_audio_source->AddRef();
    }
    ~AudioCaptureTrack() noexcept override {
		m_audio_source->Release();
    }
    // TODO(deadbeef): Figure out if the following interface should be const or
    // not.
    webrtc::AudioSourceInterface* GetSource() const override {
        return m_audio_source;
    };

    // Add/Remove a sink that will receive the audio data from the track.
    void AddSink(webrtc::AudioTrackSinkInterface* sink) override {
		m_audio_source->AddSink(sink);
    };
    virtual void RemoveSink(webrtc::AudioTrackSinkInterface* sink) override {
		m_audio_source->RemoveSink(sink);
    };

    // Get the signal level from the audio track.
    // Return true on success, otherwise false.
    // TODO(deadbeef): Change the interface to int GetSignalLevel() and pure
    // virtual after it's implemented in chromium.
    bool GetSignalLevel(int* level) override {
        return false;
    };

    // Get the audio processor used by the audio track. Return null if the track
    // does not have any processor.
    // TODO(deadbeef): Make the interface pure virtual.
    rtc::scoped_refptr<webrtc::AudioProcessorInterface> GetAudioProcessor() override{
        return nullptr;
    };

    std::string kind() const override {
        return kAudioKind;
    }

    // Track identifier.
    std::string id() const override { return track_id_; }

    // A disabled track will produce silence (if audio) or black frames (if
    // video). Can be disabled and re-enabled.
    bool enabled() const override { return enabled_; }
    bool set_enabled(bool enable) override {
        enabled_ = enable;
        return enabled_;
    }

    // Live or ended. A track will never be live again after becoming ended.
    TrackState state() const override {
        return kLive;
    };

    void RegisterObserver(webrtc::ObserverInterface* observer) override {

    }

    void UnregisterObserver(webrtc::ObserverInterface* observer) override {
    }

    void AddRef() const override { ++ref_count_; }

    webrtc::RefCountReleaseStatus Release()const override {
        int count = --ref_count_;
        if (count == 0) {
            delete this;
            return webrtc::RefCountReleaseStatus::kDroppedLastRef;
        }
        return webrtc::RefCountReleaseStatus::kOtherRefsRemained;
    }


protected:
    mutable std::atomic<int> ref_count_ = 0;
    bool enabled_;

    std::string track_id_;
    webrtc::AudioSourceInterface* m_audio_source;
};