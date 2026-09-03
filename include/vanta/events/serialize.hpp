#pragma once

#include <string>
#include <string_view>

#include "vanta/events/event.hpp"
#include "vanta/events/raw_frame.hpp"

namespace vanta {

/// Escape a string for inclusion in a JSON string literal.
///
/// Handles the quote, the backslash, and every control character below 0x20.
/// Bytes at or above 0x20 pass through unchanged, so already-valid UTF-8
/// survives untouched.
[[nodiscard]] std::string escapeJson(std::string_view text);

/// One-line JSON for each event type.
///
/// Field order is fixed and prices are written as canonical decimal strings, so
/// the same input always produces byte-identical output. That property is what
/// the golden replay test in the final chunk compares against.
[[nodiscard]] std::string toJson(const Quote& quote);
[[nodiscard]] std::string toJson(const Trade& trade);
[[nodiscard]] std::string toJson(const ConnectionState& state);
[[nodiscard]] std::string toJson(const OrderUpdate& update);
[[nodiscard]] std::string toJson(const Event& event);
[[nodiscard]] std::string toJson(const RawFrame& frame);

}  // namespace vanta
