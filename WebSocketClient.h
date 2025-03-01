// WebSocketClient.h
#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/io_context.hpp>
#include <functional>
#include <string>
#include <queue>
#include <mutex>
#include <memory>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class WebSocketClient : public std::enable_shared_from_this<WebSocketClient> {
public:
    // Enum for connection status with clear naming
    enum class ConnectionState {
        DISCONNECTED,     // Not connected yet or explicitly disconnected
        CONNECTING,       // In the process of establishing connection
        CONNECTED,        // Successfully connected
        CONNECTION_ERROR, // Failed to connect or connection lost due to error
        CLOSING           // In the process of closing the connection
    };

    // Enum for message operation status
    enum class MessageStatus {
        QUEUED,           // Message is in the queue, waiting to be sent
        SENDING,          // Message is currently being sent
        SENT,             // Message was successfully sent
        FAILED,           // Failed to send the message
        CANCELLED         // Message was cancelled (e.g., due to disconnect)
    };

    // You can also add an enum for message types if needed
    enum class MessageType {
        TEXT,             // Standard text message
        BINARY,           // Binary data message
        PING,             // Ping message for keepalive
        PONG,             // Pong response
        CLOSE             // Close message
    };

    // Struct to represent a message in the queue with its status
    struct QueuedMessage {
        std::string data;
        MessageType type;
        MessageStatus status;
        QueuedMessage():status(MessageStatus::QUEUED), type(MessageType::TEXT){}
        QueuedMessage(std::string d, MessageType t = MessageType::TEXT)
            : data(std::move(d)), type(t), status(MessageStatus::QUEUED) {
        }
    };

    // Type definitions for callbacks
    using MessageCallback = std::function<void(std::string)>;
    using StateChangeCallback = std::function<void(ConnectionState)>;
    using MessageStatusCallback = std::function<void(const std::string&, MessageStatus)>;

    WebSocketClient(net::io_context& ioc, std::string host, std::string port, std::string path);
    ~WebSocketClient();

    // Connect with state change notification
    void Connect(MessageCallback onMessage, StateChangeCallback onStateChange = nullptr);
    void Send(const std::string& message, MessageStatusCallback onStatus = nullptr);
    void Disconnect();

    // Getters for current state
    ConnectionState GetConnectionState() const;
    bool IsConnected() const; // Convenience method

private:
    void Read();
    void DoSend();
    void OnWrite(beast::error_code ec, std::size_t bytes_transferred);
    void SetConnectionState(ConnectionState newState);

    net::io_context& m_ioc;
    net::strand<net::io_context::executor_type> m_strand;
    std::unique_ptr<websocket::stream<tcp::socket>> m_ws;
    beast::flat_buffer m_f_buffer;

    std::string m_host;
    std::string m_port;
    std::string m_path;

    ConnectionState m_connectionState;

    MessageCallback m_onMessage;
    StateChangeCallback m_onStateChange;

    // Message queue with status tracking
    std::queue<std::pair<QueuedMessage, MessageStatusCallback>> m_messageQueue;
    std::mutex m_queueMutex;
};

// Utility function to convert enum to string (for logging/debugging)
inline std::string ConnectionStateToString(WebSocketClient::ConnectionState state) {
    switch (state) {
    case WebSocketClient::ConnectionState::DISCONNECTED: return "DISCONNECTED";
    case WebSocketClient::ConnectionState::CONNECTING: return "CONNECTING";
    case WebSocketClient::ConnectionState::CONNECTED: return "CONNECTED";
    case WebSocketClient::ConnectionState::CONNECTION_ERROR: return "CONNECTION_ERROR";
    case WebSocketClient::ConnectionState::CLOSING: return "CLOSING";
    default: return "UNKNOWN";
    }
}

inline std::string MessageStatusToString(WebSocketClient::MessageStatus status) {
    switch (status) {
    case WebSocketClient::MessageStatus::QUEUED: return "QUEUED";
    case WebSocketClient::MessageStatus::SENDING: return "SENDING";
    case WebSocketClient::MessageStatus::SENT: return "SENT";
    case WebSocketClient::MessageStatus::FAILED: return "FAILED";
    case WebSocketClient::MessageStatus::CANCELLED: return "CANCELLED";
    default: return "UNKNOWN";
    }
}