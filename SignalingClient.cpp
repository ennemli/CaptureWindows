#include "SignalingClient.h"
#include <iostream>
#include "CaptureSource.h" 
SignalingClient::SignalingClient(net::io_context& ioc,
    const std::string& serverUrl,
    const std::string& serverPort,
    const std::string& serverPath)
    : m_streamerId(0),
    m_serverURL(serverUrl),
    m_serverPort(serverPort),
    m_serverPath(serverPath) {

    webrtc::PeerConnectionInterface::IceServer ice_server1;
    ice_server1.uri = "stun:stun.l.google.com:19302";
    webrtc::PeerConnectionInterface::IceServer ice_server2;
    ice_server2.uri = "stun:stun1.l.google.com:19302";
    config.servers.push_back(ice_server1);
    config.servers.push_back(ice_server2);
    // Enable CPU adaptation for video quality
    config.set_cpu_adaptation(true);

    // Configure for lower latency
    config.set_dscp(true);
    // Create WebSocket client
    m_webSocket = std::make_shared<WebSocketClient>(ioc, serverUrl, serverPort, serverPath);
}

SignalingClient::~SignalingClient() {
    Disconnect();
}


void SignalingClient::Connect() {
    // Connect to WebSocket server with callbacks
    m_webSocket->Connect(
        [this](const std::string& message) {
            OnMessage(message);
        },
        [this](WebSocketClient::ConnectionState state) {
            OnConnectionStateChange(state);
        }
    );
}


void SignalingClient::Disconnect() {
    if (m_webSocket) {
        m_webSocket->Disconnect();
    }
}

WebSocketClient::ConnectionState SignalingClient::GetConnectionState() const {
    return m_webSocket ? m_webSocket->GetConnectionState() : WebSocketClient::ConnectionState::DISCONNECTED;
}

int SignalingClient::GetStreamerId() const {
    return m_streamerId;
}

void SignalingClient::SetPeerConnectionFactory(rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory) {
    m_peerConnectionFactory = peer_connection_factory;
}
void SignalingClient::SetOnPeerConnectionRequest(PeerConnectionCallBack callBack) {
    m_onPeerConnectionCallBack = std::move(callBack);
}


void SignalingClient::OnConnectionStateChange(WebSocketClient::ConnectionState state) {
    if (state == WebSocketClient::ConnectionState::CONNECTED) {
        std::cout << "Connected to signaling server" << std::endl;
    }
    else if (state == WebSocketClient::ConnectionState::DISCONNECTED) {
        std::cout << "Disconnected from signaling server" << std::endl;
    }
    else if (state == WebSocketClient::ConnectionState::CONNECTION_ERROR) {
        std::cerr << "Signaling server connection error" << std::endl;
    }
}


void SignalingClient::OnMessage(const std::string& message) {
    try {
        std::cout << message << "\n";
        boost::json::value value{ boost::json::parse(message) };
        HandleJSONMessage(value);
    }
    catch (const boost::system::system_error& e) {
        std::cerr << "JSON parsing failed: " << e.what() << std::endl;
        return;
    }
}

void SignalingClient::HandleJSONMessage(const boost::json::value& value) {
    if (value.is_object()) {
        const boost::json::object& obj = value.as_object();
        if (!obj.contains("type")) {
            std::cerr << "Message has no type field" << std::endl;
            return;
        }
        std::string type = obj.at("type").as_string().c_str();
        if (type == "connect") {
            m_streamerId = static_cast<IdType>(obj.at("id").as_int64());
            std::cout << "Connected with streamer id: " << m_streamerId << std::endl;
        }
        else if (type == "subscribe") {
            IdType sender_id = static_cast<IdType>(obj.at("sender").as_int64());

            if (m_consumers.find(sender_id) == m_consumers.end()) {

            std::cout << "Consumer wants to subscribe" << sender_id << std::endl;
            SendOffer(sender_id);
            }
            else {
                std::cout << "Consumer " << sender_id << " already connected" << std::endl;
            }
        }
        else if (type == "answer") {
            IdType sender_id = static_cast<IdType>(obj.at("sender").as_int64());
                std::string sdp = obj.at("sdp").as_string().c_str();
                std::cout << "Consumer answered" << sender_id << std::endl;
                HandleAnswer(sender_id, sdp);
           
        }
        else if (type == "consumer-disconnected") {
            IdType consumer_id =static_cast<IdType>(obj.at("consumer_id").as_int64());
            m_consumers.erase(consumer_id);
            std::cout << "Consumer disconnected: " << consumer_id << std::endl;
        }
        else if (type == "ice-candidate") {
            IdType sender_id =static_cast<IdType>(obj.at("sender").as_int64());
            boost::json::value iceCandidate = obj.at("candidate");
            std::cout << "Received ICE candidate from peer: " << sender_id << std::endl;

            ProcessIceCandidate(sender_id, iceCandidate);
        }
        else {
            std::cerr << "Unkwon message" << "\n";
        }
    }
    else {
        std::cerr << "Parsed value is not a JSON object." << std::endl;
    }
}

void SignalingClient::SendOffer(IdType consumer_id) {

    class SignalingPeerConnectionObserver : public webrtc::PeerConnectionObserver {
    public:
        SignalingPeerConnectionObserver(SignalingClient* client, IdType consumer_id) : m_client(client), consumer_id_(consumer_id) {}

        void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override {
            std::cout << "Signaling state changed to: " << new_state << std::endl;
        }

        void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override {
            std::cout << "Data channel created" << std::endl;
        }

        void OnRenegotiationNeeded() override {
            std::cout << "Renegotiation needed" << std::endl;
            // We're the answerer, so we don't create offers

            auto peer_connection_it = m_client->m_consumers.find(consumer_id_);
            if (peer_connection_it != m_client->m_consumers.end()) {
                auto peer_connection = peer_connection_it->second;

                webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
                peer_connection->CreateOffer(
                    new webrtc::RefCountedObject<CreateSDPOfferObserver>(consumer_id_, peer_connection, m_client),
                    options
                );
            
            }
            else {
                std::cerr << "Renegotiation needed but peer connection not found for consumer: " << consumer_id_ << std::endl;
            }
        }

        void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
            std::cout << "ICE connection state changed to: " << new_state << std::endl;
            if (new_state == webrtc::PeerConnectionInterface::kIceConnectionConnected) {
                std::cout << "ICE connected! Media should be flowing." << std::endl;
            }
            else if (new_state == webrtc::PeerConnectionInterface::kIceConnectionFailed) {
                std::cout << "ICE connection failed!" << std::endl;
            }
        }

        void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override {
            std::cout << "ICE gathering state changed to: " << new_state << std::endl;
        }

        void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
            std::cout << "New ICE candidate" << std::endl;
            if (m_client) {
                m_client->OnIceCandidate(consumer_id_, candidate);
            }
        }

        void OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) override {
            std::cout << "Received track (unexpected in streamer role)" << std::endl;
        }
    private:
        SignalingClient* m_client;
        IdType consumer_id_;


    };


    webrtc::PeerConnectionDependencies dependencies(new SignalingPeerConnectionObserver(this, consumer_id));
    webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::PeerConnectionInterface>> peer_connection =
        m_peerConnectionFactory->CreatePeerConnectionOrError(config, std::move(dependencies));
    if (!peer_connection.ok()) {
        std::cerr << "Failed to create PeerConnection for " << consumer_id << std::endl;
        return;
    }
    m_consumers.insert({ consumer_id,peer_connection.value() });
   /* webrtc::BitrateSettings bitrate_settings;

    // Set initial bitrate constraints
    bitrate_settings.start_bitrate_bps = 1000000;  // 1 Mbps starting bitrate
    bitrate_settings.max_bitrate_bps = 5000000;    // 5 Mbps maximum
    bitrate_settings.min_bitrate_bps = 300000;     // 300 Kbps minimum

	peer_connection.value()->SetBitrate(bitrate_settings);
    */
    // Add video track with transceiver BEFORE setting remote description
    try {


        std::string video_id = "video_track_" + std::to_string(consumer_id);
        std::string audio_id = "audio_track_" + std::to_string(consumer_id);

        std::string stream_id = "stream_" + std::to_string(consumer_id);

        webrtc::scoped_refptr<webrtc::VideoTrackInterface> video_track
        (new rtc::RefCountedObject<VideoCaptureTrack>(video_id));
        webrtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track
        (new rtc::RefCountedObject<AudioCaptureTrack>(audio_id));
        webrtc::RTCErrorOr<webrtc::scoped_refptr<webrtc::RtpSenderInterface>>	rtp_sender =
            peer_connection.value()->AddTrack(video_track, { stream_id });
        if (!rtp_sender.ok()) {
            std::cerr << "Failed to add video track to peer connection: " << rtp_sender.error().message() << std::endl;
        }
        else {
            std::cout << "Added video track " << video_id << " to peer connection" << std::endl;
        }
        rtp_sender = peer_connection.value()->AddTrack(audio_track, { stream_id });
        if (!rtp_sender.ok()) {
            std::cerr << "Failed to add audio track to peer connection: " << rtp_sender.error().message() << std::endl;

        }
        else {
            std::cout << "Added audio track " << audio_id << " to peer connection" << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception adding video track: " << e.what() << std::endl;
    }
}
void SignalingClient::HandleAnswer(IdType consumer_id,const std::string sdp) {
    auto consumer_it = m_consumers.find(consumer_id);
    if (consumer_it == m_consumers.end()) {
        std::cerr << "Peer connection not found: " << consumer_id << std::endl;
        return;
    }
    auto peer_connection = consumer_it->second;
    // Create description
	webrtc::SdpParseError* error_out=nullptr;

	std::unique_ptr<webrtc::SessionDescriptionInterface> session_description = 
        webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, sdp, error_out);
    if (error_out){
		std::cerr << "Error creating session description"<< error_out->line << std::endl;
    }

    // Create answer with proper observer
	webrtc::scoped_refptr<SetRemoteSessionDescriptionObserver> observer(
		new rtc::RefCountedObject<SetRemoteSessionDescriptionObserver>(consumer_id));
    peer_connection->SetRemoteDescription(
        std::move(session_description), observer
        );
}
void SignalingClient::ProcessIceCandidate(IdType peerId, const boost::json::value& candidate) {
    if (m_consumers.find(peerId) == m_consumers.end()) {
        std::cerr << "peer connection not found: " << peerId << std::endl;
        return;
    }
    auto peer_connection = m_consumers[peerId];

    // Create ICE candidate from JSON
    std::string sdpMid = candidate.at("sdpMid").as_string().c_str();
    int sdpMLineIndex = static_cast<int>(candidate.at("sdpMLineIndex").as_int64());
    std::string sdp = candidate.at("candidate").as_string().c_str();

    // Create and add ICE candidate
    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::IceCandidateInterface> ice_candidate(
        webrtc::CreateIceCandidate(sdpMid, sdpMLineIndex, sdp, &error));

    if (!ice_candidate) {
        std::cerr << "Failed to parse ICE candidate: " << error.description << std::endl;
        return;
    }

    // Add ICE candidate to peer connection
    if (!peer_connection->AddIceCandidate(ice_candidate.get())) {
        std::cerr << "Failed to add ICE candidate" << std::endl;
    }
}

void SignalingClient::OnIceCandidate(IdType consumer_id,const webrtc::IceCandidateInterface* candidate) {
    // Convert ICE candidate to JSON
    std::string sdp;
    candidate->ToString(&sdp);

    // Create JSON message for each peer
        boost::json::object message;
        message["type"] = "ice-candidate";
        message["target"] = consumer_id;

        boost::json::object candidateJson;
        candidateJson["sdpMid"] = candidate->sdp_mid();
        candidateJson["sdpMLineIndex"] = candidate->sdp_mline_index();
        candidateJson["candidate"] = sdp;
        message["candidate"] = candidateJson;

        // Send ICE candidate to peer
        SendToPeer(consumer_id, boost::json::serialize(message));
    
}

void SignalingClient::SendToPeer(IdType peerId, const std::string& message) {
    if (!m_webSocket || !m_webSocket->IsConnected()) {
        std::cerr << "WebSocket not connected" << std::endl;
        return;
    }
    // Send message via WebSocket
    m_webSocket->Send(message, [peerId](const std::string& msg, WebSocketClient::MessageStatus status) {
        if (status == WebSocketClient::MessageStatus::SENT) {
            std::cout << "Message sent to peer " << peerId << " successfully" << std::endl;
        }
        else if (status == WebSocketClient::MessageStatus::FAILED) {
            std::cerr << "Failed to send message to peer " << peerId << std::endl;
        }
        });
}