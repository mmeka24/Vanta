#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

#include "vanta/events/event.hpp"
#include "vanta/events/raw_frame.hpp"
#include "vanta/events/serialize.hpp"

using vanta::ConnectionState;
using vanta::ConnectionStatus;
using vanta::Event;
using vanta::FrameSource;
using vanta::OrderStatus;
using vanta::OrderUpdate;
using vanta::Price;
using vanta::Quote;
using vanta::RawFrame;
using vanta::SequenceNumber;
using vanta::Timestamp;
using vanta::Trade;

namespace {

Quote makeQuote() {
    Quote quote;
    quote.sequence = SequenceNumber{7};
    quote.event_time = Timestamp::fromNanos(1'700'000'000'123'456'789);
    quote.symbol = "AAPL";
    quote.bid = Price::fromTicks(1'902'500);
    quote.ask = Price::fromTicks(1'902'700);
    quote.bid_size = 300;
    quote.ask_size = 150;
    return quote;
}

}  // namespace

TEST_CASE("json escaping covers quotes, backslashes, and control characters",
          "[serialize][escape]") {
    CHECK(vanta::escapeJson("plain") == "plain");
    CHECK(vanta::escapeJson("he said \"hi\"") == "he said \\\"hi\\\"");
    CHECK(vanta::escapeJson("back\\slash") == "back\\\\slash");
    CHECK(vanta::escapeJson("line\nbreak") == "line\\nbreak");
    CHECK(vanta::escapeJson("tab\there") == "tab\\there");
    CHECK(vanta::escapeJson("carriage\rreturn") == "carriage\\rreturn");
    CHECK(vanta::escapeJson(std::string_view("\x01\x1f", 2)) == "\\u0001\\u001f");

    // A byte at or above 0x20 is left alone, so valid UTF-8 survives unchanged.
    CHECK(vanta::escapeJson("caf\xc3\xa9") == "caf\xc3\xa9");
}

TEST_CASE("quote serialization is byte-exact", "[serialize][quote]") {
    const std::string expected =
        R"({"type":"quote","seq":7,"event_time_ns":1700000000123456789,"symbol":"AAPL",)"
        R"("bid":"190.2500","ask":"190.2700","bid_size":300,"ask_size":150})";

    CHECK(vanta::toJson(makeQuote()) == expected);
}

TEST_CASE("trade serialization is byte-exact", "[serialize][trade]") {
    Trade trade;
    trade.sequence = SequenceNumber{8};
    trade.event_time = Timestamp::fromNanos(1'700'000'000'123'456'790);
    trade.symbol = "AAPL";
    trade.price = Price::fromTicks(1'902'600);
    trade.size = 50;

    const std::string expected =
        R"({"type":"trade","seq":8,"event_time_ns":1700000000123456790,"symbol":"AAPL",)"
        R"("price":"190.2600","size":50})";

    CHECK(vanta::toJson(trade) == expected);
}

TEST_CASE("connection state serialization is byte-exact", "[serialize][connection]") {
    ConnectionState state;
    state.sequence = SequenceNumber{1};
    state.event_time = Timestamp::fromNanos(1'700'000'000'000'000'000);
    state.status = ConnectionStatus::kSubscribed;
    state.detail = "AAPL quotes,trades";

    const std::string expected =
        R"({"type":"connection","seq":1,"event_time_ns":1700000000000000000,)"
        R"("status":"subscribed","detail":"AAPL quotes,trades"})";

    CHECK(vanta::toJson(state) == expected);
}

TEST_CASE("order update serialization is byte-exact", "[serialize][order]") {
    OrderUpdate update;
    update.sequence = SequenceNumber{9};
    update.event_time = Timestamp::fromNanos(1'700'000'000'200'000'000);
    update.client_order_id = "vanta-0001";
    update.broker_order_id = "b-42";
    update.status = OrderStatus::kPartiallyFilled;
    update.filled_quantity = 25;
    update.average_fill_price = Price::fromTicks(1'902'600);

    const std::string expected =
        R"({"type":"order_update","seq":9,"event_time_ns":1700000000200000000,)"
        R"("client_order_id":"vanta-0001","broker_order_id":"b-42","status":"partially_filled",)"
        R"("filled_quantity":25,"average_fill_price":"190.2600","reason":""})";

    CHECK(vanta::toJson(update) == expected);
}

TEST_CASE("raw frame serialization escapes an embedded json payload", "[serialize][raw]") {
    RawFrame frame;
    frame.sequence = SequenceNumber{3};
    frame.source = FrameSource::kMarketData;
    frame.arrival = Timestamp::fromNanos(1'700'000'000'000'000'001);
    frame.payload = R"([{"T":"q","S":"AAPL","bp":190.25}])";

    const std::string expected =
        R"({"seq":3,"source":"market_data","arrival_ns":1700000000000000001,)"
        R"("payload":"[{\"T\":\"q\",\"S\":\"AAPL\",\"bp\":190.25}]"})";

    CHECK(vanta::toJson(frame) == expected);
}

TEST_CASE("serialization is stable and independent of the variant wrapper",
          "[serialize][determinism]") {
    const Quote quote = makeQuote();

    // Same input, repeated calls: identical bytes.
    CHECK(vanta::toJson(quote) == vanta::toJson(quote));

    // Two separately constructed but equal quotes serialize identically.
    CHECK(vanta::toJson(quote) == vanta::toJson(makeQuote()));

    // Dispatching through the variant produces the same bytes as the direct
    // overload, so replay output does not depend on how the event was held.
    CHECK(vanta::toJson(Event{quote}) == vanta::toJson(quote));
}

TEST_CASE("negative and extreme prices serialize without losing precision", "[serialize][price]") {
    Trade trade;
    trade.sequence = SequenceNumber{1};
    trade.event_time = Timestamp::fromNanos(0);
    trade.symbol = "AAPL";
    trade.price = Price::fromTicks(-1);
    trade.size = 1;

    const std::string expected =
        R"({"type":"trade","seq":1,"event_time_ns":0,"symbol":"AAPL","price":"-0.0001","size":1})";

    CHECK(vanta::toJson(trade) == expected);
}
