#include <iostream>
#include <memory>
#include <boost/asio.hpp>

#include <api/create_peerconnection_factory.h>
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/video_codecs/video_encoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_open_h264_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_open_h264_adapter.h"

#include "SignalingClient.h"
#include <rtc_base/thread.h>

int main() {
    net::io_context ioc;

    rtc::Thread* network_thread = rtc::Thread::CreateWithSocketServer().release();
    network_thread->Start();
    rtc::Thread* worker_thread = rtc::Thread::Create().release();
    worker_thread->Start();
    rtc::Thread* signaling_thread = rtc::Thread::Create().release();
    signaling_thread->Start();

    // Create video encoder factory with H264 support using the template
    std::unique_ptr<webrtc::VideoEncoderFactory> encoder_factory =
        std::make_unique<webrtc::VideoEncoderFactoryTemplate<
        webrtc::LibvpxVp9EncoderTemplateAdapter,
        webrtc::OpenH264EncoderTemplateAdapter,
        webrtc::LibvpxVp8EncoderTemplateAdapter
        >>();

    // Create video decoder factory with H264 support using the template
    std::unique_ptr<webrtc::VideoDecoderFactory> decoder_factory =
        std::make_unique<webrtc::VideoDecoderFactoryTemplate<
        webrtc::LibvpxVp9DecoderTemplateAdapter,
        webrtc::OpenH264DecoderTemplateAdapter,
        webrtc::LibvpxVp8DecoderTemplateAdapter
        >>();

    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
        peer_connection_factory = webrtc::CreatePeerConnectionFactory(
            network_thread,
            worker_thread,
            signaling_thread,
            nullptr,
            webrtc::CreateBuiltinAudioEncoderFactory(),
            webrtc::CreateBuiltinAudioDecoderFactory(),
            std::move(encoder_factory),
            std::move(decoder_factory),
            nullptr,
            nullptr);

    if (!peer_connection_factory) {
        std::cerr << "Failed to create PeerConnectionFactory" << std::endl;
        return 1;
    }

    webrtc::scoped_refptr<SignalingClient> signaling_client(new webrtc::RefCountedObject< SignalingClient>(ioc, "localhost", "3000", "/?role=streamer"));

    signaling_client->SetPeerConnectionFactory(peer_connection_factory);

   
   

    signaling_client->Connect();

    ioc.run();
    return 0;
}
