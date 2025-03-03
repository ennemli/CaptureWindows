//SignalingClient.h
#pragma once

#include "WebSocketClient.h"
#include <api/candidate.h>
#include <api/peer_connection_interface.h>
#include <boost/json.hpp>
#include <functional>
#include <string>
#include <memory>

class IceCandidateObserver :public webrtc::PeerConnectionObserver {
public:
	explicit IceCandidateObserver(SignalingClient* client) :m_client(client) {}
	void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override {}
	void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override {}
	void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override {}

	void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
		if (m_client) {
			m_client->OnIceCandidate(candidate);
		}
	}
private:
	SignalingClient* m_client;

};

class SetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver {
public:
	void OnSuccess() override {}
	void OnFailure(webrtc::RTCError error) override {
		std::cerr << "Failed to set session description: " << error.message() << std::endl;
	}

	// RefCounted implementation
	void AddRef() const override { ++ref_count_; }
	rtc::RefCountReleaseStatus Release() const override {
		if (--ref_count_ == 0) {
			delete this;
			return rtc::RefCountReleaseStatus::kDroppedLastRef;
		}
		return rtc::RefCountReleaseStatus::kOtherRefsRemained;
	}

private:
	mutable int ref_count_ = 0;
};

class SignalingClient : std::enable_shared_from_this<SignalingClient> {
public:
	using PeerConnectionCallBack = std::function<void(int peerId)>;
	SignalingClient(net::io_context& ioc, const std::string& serverURL, const std::string& serverPort, const std::string& serverPath);
	~SignalingClient();
	void Connect();
	void Disconnect();
	void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);
	void SendOffer(int peerId);
	void SetOnPeerConnectionRequest(PeerConnectionCallBack callBack);
	int GetClientId() const;
	WebSocketClient::ConnectionState GetConnectionState() const;
private:
	void OnMessage(const std::string& message);
	void OnConnectionStateChange(WebSocketClient::ConnectionState state);
	void SendToPeer(int peerId,const boost::json::value& value);
	void HandleJSONMessage(const boost::json::value& value);
	void ProcessAnswer(int peerId, const std::string& sdp);
	void ProcessIceCandidate(int peerId, const boost::json::value& candidate);

	std::shared_ptr<WebSocketClient> m_webSocket;

	rtc::scoped_refptr<webrtc::PeerConnectionInterface> m_peerConnection;

	int m_clientId;

	std::vector<int> m_peers;

	PeerConnectionCallBack m_onPeerConnectionCallBack;

	std::string m_serverURL;
	std::string m_serverPort;
	std::string m_serverPath;
};