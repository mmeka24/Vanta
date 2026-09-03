#include <catch2/catch_test_macros.hpp>

#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "vanta/live/alpaca_config.hpp"

namespace {

std::map<std::string, std::string> validEnvironment() {
    return {{"VANTA_ALPACA_DATA_URL", "wss://stream.example.test/v2/test"},
            {"APCA_API_KEY_ID", "test-key"},
            {"APCA_API_SECRET_KEY", "test-secret"},
            {"VANTA_ALPACA_FEED", "test"},
            {"VANTA_SYMBOL", "AAPL"}};
}

auto lookup(const std::map<std::string, std::string>& environment) {
    return [&environment](std::string_view name) -> std::optional<std::string> {
        const auto found = environment.find(std::string(name));
        if (found == environment.end()) {
            return std::nullopt;
        }
        return found->second;
    };
}

}  // namespace

TEST_CASE("Alpaca configuration loads every required value", "[alpaca][config]") {
    const auto environment = validEnvironment();
    const auto result = vanta::loadAlpacaConfig(lookup(environment));

    REQUIRE(result.hasValue());
    CHECK(result.value().feed_url == "wss://stream.example.test/v2/test");
    CHECK(result.value().feed == "test");
    CHECK(result.value().symbol == "AAPL");
}

TEST_CASE("missing Alpaca configuration fails without exposing other values",
          "[alpaca][config][credentials]") {
    auto environment = validEnvironment();
    environment.erase("APCA_API_SECRET_KEY");

    const auto result = vanta::loadAlpacaConfig(lookup(environment));
    REQUIRE_FALSE(result.hasValue());
    CHECK(result.error().find("APCA_API_SECRET_KEY") != std::string::npos);
    CHECK(result.error().find("test-key") == std::string::npos);
}

TEST_CASE("Vanta V1 rejects symbols other than AAPL", "[alpaca][config]") {
    auto environment = validEnvironment();
    environment["VANTA_SYMBOL"] = "MSFT";

    const auto result = vanta::loadAlpacaConfig(lookup(environment));
    REQUIRE_FALSE(result.hasValue());
    CHECK(result.error() == "VANTA_SYMBOL must be AAPL for Vanta V1");
}
