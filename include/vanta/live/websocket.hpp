#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace vanta {

enum class WebSocketEventType : std::uint8_t {
    kOpen,
    kMessage,
    kError,
    kClosed,
};

struct WebSocketEvent {
    WebSocketEventType type{WebSocketEventType::kError};
    std::string text;
};

class IWebSocket {
  public:
    using Handler = std::function<void(const WebSocketEvent&)>;

    virtual ~IWebSocket() = default;
    virtual void setHandler(Handler handler) = 0;
    virtual void start(std::string_view url) = 0;
    [[nodiscard]] virtual bool sendText(std::string_view text) = 0;
    virtual void stop() = 0;
};

using WebSocketFactory = std::function<std::unique_ptr<IWebSocket>()>;

[[nodiscard]] std::unique_ptr<IWebSocket> makeIxWebSocket();

}  // namespace vanta
