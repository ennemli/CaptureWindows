//WebSocketClient.h
#pragma once
#include <boost/beast/core.hpp>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <string>
#include <functional>
namespace net = boost::asio;
namespace beast = boost::beast;	
namespace websocket = beast::websocket;
using tcp = boost::asio::ip::tcp;

class WebSocketClient {
private:
	std::unique_ptr<websocket::stream<tcp::socket>> m_ws;
	std::string m_host;
	std::string m_port;
	std::string m_path;
	std::function<void(std::string)> m_onMessage;
	beast::flat_buffer m_f_buffer;
	std::atomic<bool> m_isConnected ;
	net::io_context m_ioc;
public:
	WebSocketClient(net::io_context& ioc,std::string host, std::string port, std::string path) :m_ioc(ioc),m_isConnected(false), m_host(std::move(host)), m_port(std::move(port)), m_path(std::move(path)) = default;
	~WebSocketClient();
	void Connect(std::function<void(std::string)> onMessage);
	void Send(std::string message);
	void disconnect();
private:
	void read();

};