#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "vanta/live/alpaca_recorder.hpp"

using namespace std::chrono_literals;

namespace {

struct FakeSocketState {
    std::atomic<bool>* stop_requested{};
    std::size_t connection_number{0};
    std::vector<std::string> sent_messages;
};

class FakeWebSocket final : public vanta::IWebSocket {
  public:
    explicit FakeWebSocket(std::shared_ptr<FakeSocketState> state) : state_(std::move(state)) {}

    void setHandler(Handler handler) override {
        handler_ = std::move(handler);
    }

    void start(std::string_view) override {
        handler_({vanta::WebSocketEventType::kOpen, {}});
    }

    bool sendText(std::string_view text) override {
        state_->sent_messages.emplace_back(text);
        const auto message = nlohmann::json::parse(text);
        if (message["action"] == "auth") {
            handler_({vanta::WebSocketEventType::kMessage,
                      R"([{"T":"success","msg":"authenticated"}])"});
        } else if (message["action"] == "subscribe") {
            handler_({vanta::WebSocketEventType::kMessage,
                      R"([{"T":"subscription","quotes":["AAPL"],"trades":["AAPL"]}])"});
            handler_({vanta::WebSocketEventType::kMessage,
                      R"([{"T":"q","S":"AAPL","bp":190.25,"ap":190.27}])"});
            if (state_->connection_number == 1) {
                handler_({vanta::WebSocketEventType::kClosed, "test disconnect"});
            } else {
                state_->stop_requested->store(true);
            }
        }
        return true;
    }

    void stop() override {}

  private:
    std::shared_ptr<FakeSocketState> state_;
    Handler handler_;
};

std::filesystem::path freshPath() {
    auto path = std::filesystem::temp_directory_path() / "vanta_alpaca_recorder_test.ndjson";
    std::filesystem::remove(path);
    return path;
}

std::vector<nlohmann::json> readRecords(const std::filesystem::path& path) {
    std::vector<nlohmann::json> records;
    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
        records.push_back(nlohmann::json::parse(line));
    }
    return records;
}

}  // namespace

TEST_CASE("reconnect backoff doubles and remains bounded", "[alpaca][reconnect]") {
    vanta::ReconnectBackoff backoff(10ms, 40ms);

    CHECK(backoff.next() == 10ms);
    CHECK(backoff.next() == 20ms);
    CHECK(backoff.next() == 40ms);
    CHECK(backoff.next() == 40ms);

    backoff.reset();
    CHECK(backoff.next() == 10ms);
}

TEST_CASE("invalid reconnect backoff is rejected", "[alpaca][reconnect]") {
    CHECK_THROWS_AS(vanta::ReconnectBackoff(0ms, 10ms), std::invalid_argument);
    CHECK_THROWS_AS(vanta::ReconnectBackoff(20ms, 10ms), std::invalid_argument);
}

TEST_CASE("recorder reports an unavailable output before connecting", "[alpaca][recorder]") {
    const vanta::AlpacaConfig config{"wss://example.test", "key", "secret", "test", "AAPL"};
    const auto impossible = std::filesystem::temp_directory_path() /
                            "vanta-directory-that-does-not-exist" / "raw.ndjson";

    vanta::AlpacaRecorderClient recorder(
        config, impossible, []() -> std::unique_ptr<vanta::IWebSocket> { return nullptr; });
    CHECK_FALSE(recorder.isReady());
}

TEST_CASE("recorder authenticates, reconnects visibly, and drains AAPL frames",
          "[alpaca][recorder][shutdown]") {
    const auto path = freshPath();
    std::atomic<bool> stop_requested{false};
    auto state = std::make_shared<FakeSocketState>();
    state->stop_requested = &stop_requested;

    const vanta::AlpacaConfig config{"wss://example.test", "test-key", "test-secret", "test",
                                     "AAPL"};
    const auto factory = [state]() -> std::unique_ptr<vanta::IWebSocket> {
        ++state->connection_number;
        return std::make_unique<FakeWebSocket>(state);
    };

    const vanta::AlpacaRecorderOptions options{/*queue_capacity=*/2,
                                               /*initial_reconnect_delay=*/1ms,
                                               /*maximum_reconnect_delay=*/2ms};
    vanta::AlpacaRecorderClient recorder(config, path, factory, options);
    REQUIRE(recorder.isReady());
    CHECK(recorder.run(stop_requested));

    CHECK(state->connection_number == 2);
    REQUIRE(state->sent_messages.size() == 4);
    CHECK(state->sent_messages[0].find("test-secret") != std::string::npos);
    CHECK(state->sent_messages[1].find("test-secret") == std::string::npos);

    const auto records = readRecords(path);
    REQUIRE(records.size() == 4);
    const auto first_payload = nlohmann::json::parse(records[0]["payload"].get<std::string>());
    const auto gap_payload = nlohmann::json::parse(records[1]["payload"].get<std::string>());
    const auto reconnect_payload = nlohmann::json::parse(records[2]["payload"].get<std::string>());
    const auto second_payload = nlohmann::json::parse(records[3]["payload"].get<std::string>());
    CHECK(first_payload[0]["T"] == "q");
    CHECK(gap_payload["status"] == "gap");
    CHECK(reconnect_payload["status"] == "reconnecting");
    CHECK(second_payload[0]["T"] == "q");

    for (const auto& record : records) {
        const std::string serialized = record.dump();
        CHECK(serialized.find("test-key") == std::string::npos);
        CHECK(serialized.find("test-secret") == std::string::npos);
    }

    std::filesystem::remove(path);
}
