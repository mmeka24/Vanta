#include "vanta/events/event.hpp"

#include <type_traits>

#include "vanta/events/raw_frame.hpp"

namespace vanta {

std::string_view describe(FrameSource source) noexcept {
    switch (source) {
        case FrameSource::kMarketData:
            return "market_data";
        case FrameSource::kTradeUpdates:
            return "trade_updates";
    }
    return "unknown_source";
}

std::string_view describe(ConnectionStatus status) noexcept {
    switch (status) {
        case ConnectionStatus::kDisconnected:
            return "disconnected";
        case ConnectionStatus::kConnecting:
            return "connecting";
        case ConnectionStatus::kConnected:
            return "connected";
        case ConnectionStatus::kAuthenticated:
            return "authenticated";
        case ConnectionStatus::kSubscribed:
            return "subscribed";
    }
    return "unknown_status";
}

std::string_view describe(OrderStatus status) noexcept {
    switch (status) {
        case OrderStatus::kPendingNew:
            return "pending_new";
        case OrderStatus::kAccepted:
            return "accepted";
        case OrderStatus::kPartiallyFilled:
            return "partially_filled";
        case OrderStatus::kFilled:
            return "filled";
        case OrderStatus::kCanceled:
            return "canceled";
        case OrderStatus::kRejected:
            return "rejected";
        case OrderStatus::kExpired:
            return "expired";
    }
    return "unknown_status";
}

SequenceNumber sequenceOf(const Event& event) noexcept {
    return std::visit([](const auto& typed) noexcept { return typed.sequence; }, event);
}

Timestamp eventTimeOf(const Event& event) noexcept {
    return std::visit([](const auto& typed) noexcept { return typed.event_time; }, event);
}

std::string_view typeNameOf(const Event& event) noexcept {
    return std::visit(
        [](const auto& typed) noexcept -> std::string_view {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, Quote>) {
                return "quote";
            } else if constexpr (std::is_same_v<T, Trade>) {
                return "trade";
            } else if constexpr (std::is_same_v<T, ConnectionState>) {
                return "connection";
            } else {
                static_assert(std::is_same_v<T, OrderUpdate>,
                              "typeNameOf must name every alternative of Event");
                return "order_update";
            }
        },
        event);
}

}  // namespace vanta
