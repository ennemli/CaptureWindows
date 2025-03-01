// WebSocketClient.cpp
#include "WebSocketClient.h"
#include <iostream>

WebSocketClient::WebSocketClient(net::io_context& ioc, std::string host, std::string port, std::string path)
    : m_ioc(ioc),
    m_strand(net::make_strand(ioc.get_executor())),
    m_connectionState(ConnectionState::DISCONNECTED),
    m_host(std::move(host)),
    m_port(std::move(port)),
    m_path(std::move(path)), 
    m_MessagecurrentStatus(MessageStatus::QUEUED){
}

WebSocketClient::~WebSocketClient() {
    if (IsConnected()) {
        Disconnect();
    }
}

void WebSocketClient::Connect(MessageCallback onMessage, StateChangeCallback onStateChange) {
    // Set callbacks
    m_onMessage = std::move(onMessage);
    m_onStateChange = std::move(onStateChange);

    // Update state
    SetConnectionState(ConnectionState::CONNECTING);

    try {
        // Create a resolver and connect
        tcp::resolver resolver(m_ioc);
        // Look up the domain name
        auto const results = resolver.resolve(m_host, m_port);
        // Make the connection on the IP address we get from a lookup
        m_ws = std::make_unique<websocket::stream<tcp::socket>>(m_strand);
        net::connect(m_ws->next_layer(), results.begin(), results.end());

        // Set a decorator to pass the user-agent
        m_ws->set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req) {
                req.set(beast::http::field::user_agent, "WebRTC C++ Client");
            }));

        // Perform the websocket handshake
        m_ws->handshake(m_host, m_path);

        // Update state
        SetConnectionState(ConnectionState::CONNECTED);
        
        // Start reading
        Read();
    }
    catch (std::exception& e) {
        std::cerr << "WebSocket connection error: " << e.what() << std::endl;
        SetConnectionState(ConnectionState::CONNECTION_ERROR);
    }
}

void WebSocketClient::Send(const std::string& message, MessageStatusCallback onStatus) {
    // Check if the connection is open
    if (!IsConnected()) {
        if (onStatus) {
            m_MessagecurrentStatus = MessageStatus::CANCELLED;
            onStatus(message, m_MessagecurrentStatus);
        }
        return;
    }

    // Push the message to the queue
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_messageQueue.push({ QueuedMessage(message), onStatus });
    }

    // Post to the strand to process the queue
    net::post(m_strand, beast::bind_front_handler(
        &WebSocketClient::DoSend,
        shared_from_this()
    ));
}

void WebSocketClient::DoSend() {
    if (m_MessagecurrentStatus==MessageStatus::SENDING) {
        return;
    }
    // Check if there are messages to send
    std::pair<QueuedMessage, MessageStatusCallback> currentMsg;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_messageQueue.empty()) {
            return;
        }
        currentMsg = m_messageQueue.front();
        // Update message status
        m_MessagecurrentStatus = MessageStatus::SENDING;

        currentMsg.first.status = m_MessagecurrentStatus;

        if (currentMsg.second) {
            currentMsg.second(currentMsg.first.data, m_MessagecurrentStatus);
        }

    }
    auto self = shared_from_this();
    // Send the message
    m_ws->async_write(
        net::buffer(currentMsg.first.data),
        beast::bind_front_handler(
            &WebSocketClient::OnWrite,
           self
        )
    );
}

void WebSocketClient::OnWrite(beast::error_code ec, std::size_t bytes_transferred) {
    // Get the message and its callback
    std::pair<QueuedMessage, MessageStatusCallback> currentMsg;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (!m_messageQueue.empty()) {
            currentMsg = m_messageQueue.front();
            m_messageQueue.pop();
        }
    }

    if (ec) {
        std::cerr << "Write error: " << ec.message() << std::endl;

        // Update message status
        if (currentMsg.second) {
            m_MessagecurrentStatus = MessageStatus::FAILED;
            currentMsg.first.status = m_MessagecurrentStatus;
            currentMsg.second(currentMsg.first.data, m_MessagecurrentStatus);
        }

        SetConnectionState(ConnectionState::CONNECTION_ERROR);
        return;
    }

    std::cout << "Message sent successfully: " << bytes_transferred << std::endl;

    // Update message status
    if (currentMsg.second) {
        m_MessagecurrentStatus = MessageStatus::SENT;
        currentMsg.first.status = m_MessagecurrentStatus;
        currentMsg.second(currentMsg.first.data, m_MessagecurrentStatus);
    }

    // Check if there are more messages to send
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_messageQueue.empty()) {
            return;
        }
    }

    // Send the next message
    DoSend();
}

void WebSocketClient::Disconnect() {
    // Check if the connection is open
    if (m_connectionState != ConnectionState::CONNECTED || !m_ws) {
        return;
    }

    // Update state
    SetConnectionState(ConnectionState::CLOSING);

    // Cancel all pending messages
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        while (!m_messageQueue.empty()) {
            auto& msg = m_messageQueue.front();
            if (msg.second) {
                m_MessagecurrentStatus = MessageStatus::CANCELLED;

                msg.second(msg.first.data, MessageStatus::CANCELLED);
            }
            m_messageQueue.pop();
        }
    }

    // Post the disconnect operation to the strand
    net::post(m_strand, [self = shared_from_this()]() {
        self->m_ws->async_close(websocket::close_code::normal,
            [self](beast::error_code ec) {
                if (ec) {
                    std::cerr << "Close error: " << ec.message() << std::endl;
                    self->SetConnectionState(ConnectionState::CONNECTION_ERROR);
                }
                else {
                    self->SetConnectionState(ConnectionState::DISCONNECTED);
                    std::cout << "Connection closed successfully" << std::endl;
                }
            });
        });
}

WebSocketClient::ConnectionState WebSocketClient::GetConnectionState() const {
    return m_connectionState;
}

bool WebSocketClient::IsConnected() const {
    return m_connectionState == ConnectionState::CONNECTED;
}

void WebSocketClient::SetConnectionState(ConnectionState newState) {
    // Only notify if state actually changed
    if (m_connectionState != newState) {
        m_connectionState = newState;

        // Notify state change listener if registered
        if (m_onStateChange) {
            m_onStateChange(m_connectionState);
        }
    }
}

void WebSocketClient::Read() {
    // Check if the connection is open
    if (!IsConnected() || !m_ws) {
        return;
    }

    // Post the read operation to the strand
    m_ws->async_read(
        m_f_buffer,
        beast::bind_front_handler(
            [this](beast::error_code ec, std::size_t bytes_transferred) {
                if (ec) {
                    std::cerr << "Read error: " << ec.message() << std::endl;
                    SetConnectionState(ConnectionState::CONNECTION_ERROR);
                    return;
                }

                std::string message = beast::buffers_to_string(m_f_buffer.data());
                m_f_buffer.consume(m_f_buffer.size());

                if (m_onMessage) {
                    m_onMessage(message);
                }

                if (IsConnected()) {
                    Read();
                }
            }
        )
    );
}