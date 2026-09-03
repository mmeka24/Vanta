#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "vanta/live/alpaca_config.hpp"

namespace vanta {

enum class AlpacaMessageKind : std::uint8_t {
    kControl,
    kAuthenticated,
    kSubscribed,
    kMarketData,
    kError,
    kMalformed,
};

struct AlpacaMessageClassification {
    AlpacaMessageKind kind{AlpacaMessageKind::kMalformed};
    std::string detail;
};

/// Constructs outbound protocol messages. Callers must never log or record the
/// returned authentication message.
[[nodiscard]] std::string makeAlpacaAuthMessage(const AlpacaConfig& config);
[[nodiscard]] std::string makeAlpacaSubscriptionMessage(const AlpacaConfig& config);

/// Performs only the minimum envelope inspection needed by the recorder. Full
/// market-event parsing intentionally belongs to Chunk 4.
[[nodiscard]] AlpacaMessageClassification classifyAlpacaMessage(std::string_view payload,
                                                                std::string_view symbol);

}  // namespace vanta
