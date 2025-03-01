#include "WebSocketClient.h"
#include <iostream>
int main() {
    net::io_context ioc;
    auto ws = std::make_shared<WebSocketClient>(ioc, "localhost", "3000", "/");
    std::cout << "Establishing connection..." << "\n";
    ws->Connect(
        [](std::string message) {
            std::cout << "Received message: " << message << std::endl;
        },
        [ws](WebSocketClient::ConnectionState state) {
            std::cout << "Connection state changed to: " << ConnectionStateToString(state) << std::endl;
            if (state == WebSocketClient::ConnectionState::CONNECTED) {
                ws->Send("{\"type\":\"new-peer\" }", [](const std::string& msg, WebSocketClient::MessageStatus status) {
                    std::cout << "Message status updated: " << MessageStatusToString(status) << std::endl;
                    });
                std::this_thread::sleep_for(std::chrono::seconds(2));

                ws->Send("{\"type\":\"null\" }");
                std::this_thread::sleep_for(std::chrono::seconds(1));
                ws->Send("{\"type\":\"undefined\" }");
                
            }
        }
    );
    net::post(ioc, [&ws]() {
        std::this_thread::sleep_for(std::chrono::seconds(8));
        ws->Disconnect();
        });
    std::thread t([&ioc]{    ioc.run();
        });
    t.join();
    return 0;
}