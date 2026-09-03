#include "vanta/live/alpaca_protocol.hpp"

#include <nlohmann/json.hpp>

#include <exception>

namespace vanta {

std::string makeAlpacaAuthMessage(const AlpacaConfig& config) {
    return nlohmann::json{{"action", "auth"}, {"key", config.key_id}, {"secret", config.secret_key}}
        .dump();
}

std::string makeAlpacaSubscriptionMessage(const AlpacaConfig& config) {
    return nlohmann::json{{"action", "subscribe"},
                          {"quotes", nlohmann::json::array({config.symbol})},
                          {"trades", nlohmann::json::array({config.symbol})}}
        .dump();
}

AlpacaMessageClassification classifyAlpacaMessage(std::string_view payload,
                                                  std::string_view symbol) {
    try {
        const nlohmann::json document = nlohmann::json::parse(payload);
        if (!document.is_array()) {
            return {AlpacaMessageKind::kMalformed, "expected a JSON array"};
        }

        bool has_market_data = false;
        bool authenticated = false;
        bool subscribed = false;

        for (const auto& item : document) {
            if (!item.is_object() || !item.contains("T") || !item["T"].is_string()) {
                return {AlpacaMessageKind::kMalformed, "message is missing string field T"};
            }

            const std::string type = item["T"].get<std::string>();
            if (type == "q" || type == "t") {
                if (!item.contains("S") || !item["S"].is_string() ||
                    item["S"].get<std::string>() != symbol) {
                    return {AlpacaMessageKind::kError,
                            "received market data for an unexpected symbol"};
                }
                has_market_data = true;
            } else if (type == "success" && item.value("msg", std::string{}) == "authenticated") {
                authenticated = true;
            } else if (type == "subscription") {
                subscribed = true;
            } else if (type == "error") {
                return {AlpacaMessageKind::kError,
                        item.value("msg", std::string{"Alpaca protocol error"})};
            }
        }

        if (has_market_data) {
            return {AlpacaMessageKind::kMarketData, {}};
        }
        if (authenticated) {
            return {AlpacaMessageKind::kAuthenticated, {}};
        }
        if (subscribed) {
            return {AlpacaMessageKind::kSubscribed, {}};
        }
        return {AlpacaMessageKind::kControl, {}};
    } catch (const nlohmann::json::exception&) {
        return {AlpacaMessageKind::kMalformed, "invalid JSON from market-data socket"};
    }
}

}  // namespace vanta
