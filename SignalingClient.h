//SignalingClient.h
#pragma once

#include "WebSocketClient.h"
#include <api/candidate.h>
#include <api/peer_connection_interface.h>
#include <boost/json.hpp>
#include <functional>
#include <string>
#include <memory>
#include <iostream>
#include <atomic>
#include <rtc_base/synchronization/mutex.h>

class SetLocalDescriptionObserverInterface : public webrtc::SetLocalDescriptionObserverInterface {
public:
	void OnSetLocalDescriptionComplete(webrtc::RTCError error) override {
		if (!error.ok()) {
			std::cerr << "Failed to set local description: " << error.message() << std::endl;
		}
	}

	// RefCounted implementation
	void AddRef() const override { ++ref_count_; }
	webrtc::RefCountReleaseStatus Release() const override {
		if (--ref_count_ == 0) {
			delete this;
			return webrtc::RefCountReleaseStatus::kDroppedLastRef;
		}
		return webrtc::RefCountReleaseStatus::kOtherRefsRemained;
	}

private:
	mutable std::atomic<int> ref_count_{ 0 };
};

class SignalingClient :public webrtc::RefCountInterface {
public:
	using PeerConnectionCallBack = std::function<void(int consumerId)>;
	using IdType= uint64_t;
	SignalingClient(net::io_context& ioc, const std::string& serverURL, const std::string& serverPort, const std::string& serverPath);
	~SignalingClient();
	
	void Connect();
	void Disconnect();

	void SetOnPeerConnectionRequest(PeerConnectionCallBack callBack);
	void SetPeerConnectionFactory(rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory);


	WebSocketClient::ConnectionState GetConnectionState() const;
	int GetStreamerId() const;

	void AddRef() const override { ++ref_count_; }
	webrtc::RefCountReleaseStatus Release() const override {
		if (--ref_count_ == 0) {
			delete this;
			return webrtc::RefCountReleaseStatus::kDroppedLastRef;
		}
		return webrtc::RefCountReleaseStatus::kOtherRefsRemained;
	}

private:
	class SetRemoteSessionDescriptionObserver : public webrtc::SetRemoteDescriptionObserverInterface {
	public:
		SetRemoteSessionDescriptionObserver(IdType consumer_id) :consumer_id_(consumer_id) {}
		void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override {
			if (!error.ok()) {
				std::cerr << "Failed to set remote description: " << error.message() << std::endl;
			}
			else {
				std::cout << "Remote description set successfully for consumer: " << consumer_id_ << std::endl;
			}
		}
		// RefCounted implementation
		void AddRef() const override { ++ref_count_; }
		webrtc::RefCountReleaseStatus Release() const override {
			if (--ref_count_ == 0) {
				delete this;
				return webrtc::RefCountReleaseStatus::kDroppedLastRef;
			}
			return webrtc::RefCountReleaseStatus::kOtherRefsRemained;
		}

	private:
		IdType consumer_id_;
		mutable std::atomic<int> ref_count_ = 0;
	};
	class CreateSDPOfferObserver
		: public webrtc::CreateSessionDescriptionObserver {
	public:
		CreateSDPOfferObserver(
			IdType consumer_id,
			rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection,
			SignalingClient* client)
			: m_consumer_id(consumer_id),
			m_peer_connection(peer_connection),
			m_client(client) {
			// Capture a reference to the client to ensure it stays alive
			m_client->AddRef();
		}

		~CreateSDPOfferObserver() {
			// Release the reference when this observer is destroyed
			if (m_client) {
				m_client->Release();
			}
		}

		void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
			// Handle null description
			if (!desc) {
				std::cerr << "Created session description is null" << std::endl;
				return;
			}

			// Get SDP string
			std::string sdp;
			if (!desc->ToString(&sdp)) {
				std::cerr << "Failed to serialize session description to string" << std::endl;
				return;
			}

			// First send answer to peer, before setting local description
			// This ensures we don't have race conditions with ICE candidates
			if (m_client) {
				boost::json::object message;
				message["type"] = "offer";
				message["target"] = m_consumer_id;
				message["sdp"] = sdp;

				m_client->SendToPeer(m_consumer_id, boost::json::serialize(message));
			}

			// Set local description
			if (m_peer_connection && desc) {
				webrtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer(
					new webrtc::RefCountedObject<SetLocalDescriptionObserverInterface>());
				webrtc::MutexLock lock(&mutex_);
				m_peer_connection->SetLocalDescription(desc->Clone(), observer);
			}
			else {
				std::cerr << "Invalid peer connection or session description." << std::endl;
			}
		}

		void OnFailure(webrtc::RTCError error) override {
			std::cerr << "Failed to create answer for consumer " << m_consumer_id
				<< ": " << error.message() << std::endl;
		}

	private:
		webrtc::Mutex mutex_;
		IdType m_consumer_id;
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> m_peer_connection;
		SignalingClient* m_client;  // Raw pointer with explicit reference counting
	};

	void SendOffer(IdType consumer_id);
	void HandleAnswer(IdType consumer_id, const std::string sdp);
	void OnMessage(const std::string& message);
	void SendToPeer(IdType peerId, const std::string& message);
	void HandleJSONMessage(const boost::json::value& value);
	void ProcessIceCandidate(IdType peerId, const boost::json::value& candidate);
	void OnConnectionStateChange(WebSocketClient::ConnectionState state);
	void OnIceCandidate(IdType consumer_id, const webrtc::IceCandidateInterface* candidate);

	std::shared_ptr<WebSocketClient> m_webSocket;

	rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> m_peerConnectionFactory;

	IdType m_streamerId;

	PeerConnectionCallBack m_onPeerConnectionCallBack;

	std::map<IdType,webrtc::scoped_refptr<webrtc::PeerConnectionInterface>> m_consumers;

	std::string m_serverURL;
	std::string m_serverPort;
	std::string m_serverPath;

	webrtc::PeerConnectionInterface::RTCConfiguration config;

	mutable std::atomic<int> ref_count_ = 0;

};





