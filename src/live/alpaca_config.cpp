#include "vanta/live/alpaca_config.hpp"

#include <array>
#include <cstdlib>
#include <utility>

namespace vanta {

namespace {

std::optional<std::string> requireValue(const EnvironmentLookup& lookup, std::string_view name) {
    auto value = lookup(name);
    if (!value.has_value() || value->empty()) {
        return std::nullopt;
    }
    return value;
}

}  // namespace

Result<AlpacaConfig, std::string> loadAlpacaConfig(const EnvironmentLookup& lookup) {
    constexpr std::array<std::string_view, 5> kVariables{"VANTA_ALPACA_DATA_URL", "APCA_API_KEY_ID",
                                                         "APCA_API_SECRET_KEY", "VANTA_ALPACA_FEED",
                                                         "VANTA_SYMBOL"};

    std::array<std::string, kVariables.size()> values;
    for (std::size_t index = 0; index < kVariables.size(); ++index) {
        auto value = requireValue(lookup, kVariables[index]);
        if (!value.has_value()) {
            return Result<AlpacaConfig, std::string>::err(
                "missing required environment variable: " + std::string(kVariables[index]));
        }
        values[index] = std::move(*value);
    }

    if (values[4] != "AAPL") {
        return Result<AlpacaConfig, std::string>::err("VANTA_SYMBOL must be AAPL for Vanta V1");
    }

    AlpacaConfig config{std::move(values[0]), std::move(values[1]), std::move(values[2]),
                        std::move(values[3]), std::move(values[4])};
    return Result<AlpacaConfig, std::string>::ok(std::move(config));
}

Result<AlpacaConfig, std::string> loadAlpacaConfigFromEnvironment() {
    return loadAlpacaConfig([](std::string_view name) -> std::optional<std::string> {
        const std::string variable{name};
        const char* value = std::getenv(variable.c_str());
        if (value == nullptr) {
            return std::nullopt;
        }
        return std::string(value);
    });
}

}  // namespace vanta
