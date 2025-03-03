#include "SignalingClient.h"
#include <iostream>
SignalingClient::SignalingClient(net::io_context& ioc,
    const std::string& serverUrl,
    const std::string& serverPort,
    const std::string& serverPath)
    : m_clientId(0),
    m_serverURL(serverUrl),
    m_serverPort(serverPort),
    m_serverPath(serverPath) {

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

int SignalingClient::GetClientId() const {
    return m_clientId;
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
        boost::json::value value = boost::json::parse(message);
        HandleJSONMessage(value);
    }
    catch (const boost::json::system_error& e) {
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
            m_clientId = obj.at("clientID").as_int64();
            std::cout << "Connected with client ID: " << m_clientId << std::endl;

            m_peers.clear();
            boost::json::array peers = obj.at("peers").as_array();
            for (const auto& peer : peers) {
                m_peers.push_back(peer.as_int64());
            };
            std::cout << "Available peers: " << m_peers.size() << std::endl;
        }
        else if (type == "new-peer") {
            int peerId = obj.at("peerID").as_int64();
            m_peers.push_back(peerId);
            std::cout << "New peer connected: " << peerId << std::endl;
            if (m_onPeerConnectionCallBack) {
                m_onPeerConnectionCallBack(peerId);
            }
        }
        else if (type == "peer-disconnected") {
            int peerId = obj.at("peerID").as_int64();
            std::cout << "Peer disconnected: " << peerId << std::endl;
            m_peers.erase(std::remove(m_peers.begin(), m_peers.end(), peerId), m_peers.end());
        }
        else if (type=="answer") {
            int peerId = obj.at("peerID").as_int64();
            std::string sdp = obj.at("sdp").as_string().c_str();

            std::cout << "Received answer from peer: " << peerId << std::endl;
            ProcessAnswer(peerId, sdp);
        }
        else if (type == "ice-candidate") {
            int peerId = obj.at("sender").as_int64();
           boost::json::value iceCandidate= obj.at("candidate");
            std::cout << "Received ICE candidate from peer: " << peerId << std::endl;

            ProcessIceCandidate(peerId, iceCandidate);
        }
    }
    else {
        std::cerr << "Parsed value is not a JSON object." << std::endl;
    }
}

void SignalingClient::SendOffer(int peerId){
    if (!m_peerConnection) {
        std::cerr << "No peer connection set" << std::endl;
        return;
    }
    std::cout << "Creating offer for peer: " << peerId << std::endl;
   
    class CreateOfferObserver :public webrtc::CreateSessionDescriptionObserver {
    public:
        CreateOfferObserver(SignalingClient*client,int peerId):m_client(client),m_peerId(peerId){}
        void OnSuccess(webrtc::SessionDescriptionInterface* desc) override{
            std::string sdp;
            desc->ToString(&sdp);
            rtc::RefCountedObject<SetSessionDescriptionObserver>* sessionDescriptionObserver =
                new rtc::RefCountedObject<SetSessionDescriptionObserver>();
            m_client->m_peerConnection->SetLocalDescription(
                sessionDescriptionObserver, desc);
            boost::json::object msgJson;
            msgJson["type"] = "offer";
            msgJson["sdp"] = sdp;
            msgJson["target"] = m_peerId;
            m_client->SendToPeer(m_peerId, msgJson);
        }
        void OnFailure(webrtc::RTCError err) override{
            std::cerr << "Failed to create answer: " << err.message() << std::endl;
        }
        void AddRef() const override { ++ref_count_; }
        rtc::RefCountReleaseStatus Release() const override {
            if (--ref_count_ == 0) {
                delete this;
                return rtc::RefCountReleaseStatus::kDroppedLastRef;
            }
            return rtc::RefCountReleaseStatus::kOtherRefsRemained;
        }
    private:
        SignalingClient* m_client;
        int m_peerId;
        mutable int ref_count_ = 0;

    };

    rtc::scoped_refptr<CreateOfferObserver> 
        createOfferObserver(new rtc::RefCountedObject<CreateOfferObserver>(this, peerId));

    m_peerConnection->CreateOffer(
        createOfferObserver.get(),
        webrtc::PeerConnectionInterface::RTCOfferAnswerOptions()
    );
}

void SignalingClient::ProcessAnswer(int peerId, const std::string& sdp) {
    if (!m_peerConnection) {
        std::cerr << "No peer connection set" << std::endl;
        return;
    }

    // Create session description from SDP
    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::SessionDescriptionInterface> session_description(
        webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, sdp, &error));

    if (!session_description) {
        std::cerr << "Failed to parse SDP: " << error.description << std::endl;
        return;
    }

    // Set remote description
    m_peerConnection->SetRemoteDescription(
        new rtc::RefCountedObject<SetSessionDescriptionObserver>(),
        session_description.release());
}

void SignalingClient::ProcessIceCandidate(int peerId, const boost::json::value& candidate) {
    if (!m_peerConnection) {
        std::cerr << "No peer connection set" << std::endl;
        return;
    }

    // Create ICE candidate from JSON
    std::string sdpMid = candidate.at("sdpMid").as_string().c_str();
    int sdpMLineIndex = candidate.at("sdpMLineIndex").as_int64();
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
    if (!m_peerConnection->AddIceCandidate(ice_candidate.get())) {
        std::cerr << "Failed to add ICE candidate" << std::endl;
    }
}

void SignalingClient::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
    // Convert ICE candidate to JSON
    std::string sdp;
    candidate->ToString(&sdp);

    // Create JSON message for each peer
    for (int peerId : m_peers) {
        boost::json::object message;
        message["type"] = "ice-candidate";
        message["target"] = peerId;

        boost::json::object candidateJson;
        candidateJson["sdpMid"] = candidate->sdp_mid();
        candidateJson["sdpMLineIndex"] = candidate->sdp_mline_index();
        candidateJson["candidate"] = sdp;
        message["candidate"] = candidateJson;

        // Send ICE candidate to peer
        SendToPeer(peerId, message);
    }
}

void SignalingClient::SendToPeer(int peerId, const boost::json::value& value) {
    if (!m_webSocket || !m_webSocket->IsConnected()) {
        std::cerr << "WebSocket not connected" << std::endl;
        return;
    }
    // Send message via WebSocket
    m_webSocket->Send(boost::json::serialize(value), [peerId](const std::string& msg, WebSocketClient::MessageStatus status) {
        if (status == WebSocketClient::MessageStatus::SENT) {
            std::cout << "Message sent to peer " << peerId << " successfully" << std::endl;
        }
        else if (status == WebSocketClient::MessageStatus::FAILED) {
            std::cerr << "Failed to send message to peer " << peerId << std::endl;
        }
        });
}