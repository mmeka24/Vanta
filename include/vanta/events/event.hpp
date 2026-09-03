#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

#include "vanta/core/price.hpp"
#include "vanta/core/types.hpp"

namespace vanta {

/// Top of book after an exchange quote update.
struct Quote {
    SequenceNumber sequence{};
    Timestamp event_time{};
    Symbol symbol;
    Price bid{};
    Price ask{};
    Quantity bid_size{0};
    Quantity ask_size{0};

    friend bool operator==(const Quote&, const Quote&) = default;
};

/// One executed trade printed by the exchange.
struct Trade {
    SequenceNumber sequence{};
    Timestamp event_time{};
    Symbol symbol;
    Price price{};
    Quantity size{0};

    friend bool operator==(const Trade&, const Trade&) = default;
};

enum class ConnectionStatus : std::uint8_t {
    kDisconnected = 0,
    kConnecting = 1,
    kConnected = 2,
    kAuthenticated = 3,
    kSubscribed = 4,
};

[[nodiscard]] std::string_view describe(ConnectionStatus status) noexcept;

/// A change in the state of an external connection.
///
/// This is an event rather than a side channel because the engine must be able
/// to stop trading on a disconnect, and replay must reproduce that decision at
/// exactly the same point in the stream.
struct ConnectionState {
    SequenceNumber sequence{};
    Timestamp event_time{};
    ConnectionStatus status{ConnectionStatus::kDisconnected};
    /// Human-readable reason. Never holds credentials or authentication payloads.
    std::string detail;

    friend bool operator==(const ConnectionState&, const ConnectionState&) = default;
};

/// Lifecycle states an order can report. The legal transitions between them,
/// and the state machine that enforces them, arrive with the broker chunk.
enum class OrderStatus : std::uint8_t {
    kPendingNew = 0,
    kAccepted = 1,
    kPartiallyFilled = 2,
    kFilled = 3,
    kCanceled = 4,
    kRejected = 5,
    kExpired = 6,
};

[[nodiscard]] std::string_view describe(OrderStatus status) noexcept;

/// An asynchronous report about an order, from either broker implementation.
struct OrderUpdate {
    SequenceNumber sequence{};
    Timestamp event_time{};
    /// Vanta-generated identifier, used for idempotency across reconnects.
    std::string client_order_id;
    /// Broker-assigned identifier. Empty until the broker acknowledges.
    std::string broker_order_id;
    OrderStatus status{OrderStatus::kPendingNew};
    /// Cumulative quantity filled so far, not the size of this single fill.
    Quantity filled_quantity{0};
    /// Average fill price for filled_quantity. Zero when nothing has filled.
    Price average_fill_price{};
    /// Rejection or cancellation reason. Empty otherwise.
    std::string reason;

    friend bool operator==(const OrderUpdate&, const OrderUpdate&) = default;
};

/// Every typed event the engine can process.
///
/// Live and replay sources both produce this exact type, which is what lets one
/// engine implementation serve both modes.
using Event = std::variant<Quote, Trade, ConnectionState, OrderUpdate>;

[[nodiscard]] SequenceNumber sequenceOf(const Event& event) noexcept;
[[nodiscard]] Timestamp eventTimeOf(const Event& event) noexcept;

/// Stable type tag used in serialized output and log messages.
[[nodiscard]] std::string_view typeNameOf(const Event& event) noexcept;

}  // namespace vanta
