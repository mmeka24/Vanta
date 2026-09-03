#include "vanta/live/websocket.hpp"

#include <ixwebsocket/IXWebSocket.h>

#include <utility>

namespace vanta {

namespace {

class IxWebSocket final : public IWebSocket {
  public:
    void setHandler(Handler handler) override {
        handler_ = std::move(handler);
        socket_.setOnMessageCallback([this](const ix::WebSocketMessagePtr& message) {
            if (!handler_) {
                return;
            }
            switch (message->type) {
                case ix::WebSocketMessageType::Open:
                    handler_({WebSocketEventType::kOpen, {}});
                    break;
                case ix::WebSocketMessageType::Message:
                    handler_({WebSocketEventType::kMessage, message->str});
                    break;
                case ix::WebSocketMessageType::Error:
                    handler_({WebSocketEventType::kError, message->errorInfo.reason});
                    break;
                case ix::WebSocketMessageType::Close:
                    handler_({WebSocketEventType::kClosed, message->closeInfo.reason});
                    break;
                default:
                    break;
            }
        });
    }

    void start(std::string_view url) override {
        socket_.setUrl(std::string(url));
        socket_.disableAutomaticReconnection();
        socket_.start();
    }

    bool sendText(std::string_view text) override {
        return socket_.sendText(std::string(text)).success;
    }

    void stop() override {
        socket_.stop();
    }

  private:
    ix::WebSocket socket_;
    Handler handler_;
};

}  // namespace

std::unique_ptr<IWebSocket> makeIxWebSocket() {
    return std::make_unique<IxWebSocket>();
}

}  // namespace vanta
