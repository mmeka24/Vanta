#include <catch2/catch_test_macros.hpp>

#include <string>

#include "vanta/live/alpaca_protocol.hpp"

namespace {

vanta::AlpacaConfig config() {
    return {"wss://stream.example.test/v2/test", "visible-test-key", "private-test-secret", "test",
            "AAPL"};
}

}  // namespace

TEST_CASE("authentication and subscription messages have narrow scopes", "[alpaca][protocol]") {
    const auto value = config();
    const std::string auth = vanta::makeAlpacaAuthMessage(value);
    const std::string subscription = vanta::makeAlpacaSubscriptionMessage(value);

    CHECK(auth.find("visible-test-key") != std::string::npos);
    CHECK(auth.find("private-test-secret") != std::string::npos);
    CHECK(subscription.find("AAPL") != std::string::npos);
    CHECK(subscription.find("quotes") != std::string::npos);
    CHECK(subscription.find("trades") != std::string::npos);
    CHECK(subscription.find("private-test-secret") == std::string::npos);
}

TEST_CASE("protocol classification separates control and AAPL market frames",
          "[alpaca][protocol]") {
    using vanta::AlpacaMessageKind;

    CHECK(vanta::classifyAlpacaMessage(R"([{"T":"success","msg":"authenticated"}])", "AAPL").kind ==
          AlpacaMessageKind::kAuthenticated);
    CHECK(
        vanta::classifyAlpacaMessage(R"([{"T":"subscription","quotes":["AAPL"]}])", "AAPL").kind ==
        AlpacaMessageKind::kSubscribed);
    CHECK(vanta::classifyAlpacaMessage(R"([{"T":"q","S":"AAPL"}])", "AAPL").kind ==
          AlpacaMessageKind::kMarketData);
    CHECK(vanta::classifyAlpacaMessage(R"([{"T":"t","S":"AAPL"}])", "AAPL").kind ==
          AlpacaMessageKind::kMarketData);
}

TEST_CASE("unexpected symbols and malformed frames fail closed", "[alpaca][protocol]") {
    using vanta::AlpacaMessageKind;

    CHECK(vanta::classifyAlpacaMessage(R"([{"T":"q","S":"MSFT"}])", "AAPL").kind ==
          AlpacaMessageKind::kError);
    CHECK(vanta::classifyAlpacaMessage("not-json", "AAPL").kind == AlpacaMessageKind::kMalformed);
    CHECK(vanta::classifyAlpacaMessage(R"({"T":"q","S":"AAPL"})", "AAPL").kind ==
          AlpacaMessageKind::kMalformed);
}
