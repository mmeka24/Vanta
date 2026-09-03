#include <catch2/catch_test_macros.hpp>

#include <string>
#include <variant>

#include "vanta/events/event.hpp"
#include "vanta/events/raw_frame.hpp"

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

Trade makeTrade() {
    Trade trade;
    trade.sequence = SequenceNumber{8};
    trade.event_time = Timestamp::fromNanos(1'700'000'000'123'456'790);
    trade.symbol = "AAPL";
    trade.price = Price::fromTicks(1'902'600);
    trade.size = 50;
    return trade;
}

ConnectionState makeConnectionState() {
    ConnectionState state;
    state.sequence = SequenceNumber{1};
    state.event_time = Timestamp::fromNanos(1'700'000'000'000'000'000);
    state.status = ConnectionStatus::kSubscribed;
    state.detail = "AAPL quotes,trades";
    return state;
}

OrderUpdate makeOrderUpdate() {
    OrderUpdate update;
    update.sequence = SequenceNumber{9};
    update.event_time = Timestamp::fromNanos(1'700'000'000'200'000'000);
    update.client_order_id = "vanta-0001";
    update.broker_order_id = "b-42";
    update.status = OrderStatus::kPartiallyFilled;
    update.filled_quantity = 25;
    update.average_fill_price = Price::fromTicks(1'902'600);
    return update;
}

}  // namespace

TEST_CASE("sequence numbers compare and increment", "[types][sequence]") {
    SequenceNumber sequence{10};
    CHECK(sequence.value() == 10);
    CHECK(sequence == SequenceNumber{10});
    CHECK(sequence < SequenceNumber{11});

    CHECK((++sequence).value() == 11);
    CHECK((sequence++).value() == 11);
    CHECK(sequence.value() == 12);
}

TEST_CASE("timestamps store nanoseconds and subtract to a duration", "[types][timestamp]") {
    const Timestamp earlier = Timestamp::fromNanos(1'700'000'000'000'000'000);
    const Timestamp later = Timestamp::fromNanos(1'700'000'000'250'000'000);

    CHECK(earlier.nanos() == 1'700'000'000'000'000'000);
    CHECK(earlier < later);
    CHECK((later - earlier).count() == 250'000'000);
    CHECK(earlier + vanta::Duration{250'000'000} == later);
    CHECK(Timestamp{} == Timestamp::fromNanos(0));
}

TEST_CASE("quotes compare field by field", "[events][equality]") {
    const Quote original = makeQuote();
    CHECK(original == makeQuote());

    Quote differentBid = makeQuote();
    differentBid.bid = Price::fromTicks(1'902'400);
    CHECK(original != differentBid);

    Quote differentSize = makeQuote();
    differentSize.ask_size = 151;
    CHECK(original != differentSize);

    Quote differentSequence = makeQuote();
    differentSequence.sequence = SequenceNumber{8};
    CHECK(original != differentSequence);

    Quote differentTime = makeQuote();
    differentTime.event_time = Timestamp::fromNanos(1'700'000'000'123'456'788);
    CHECK(original != differentTime);
}

TEST_CASE("trades, connection states, and order updates compare field by field",
          "[events][equality]") {
    CHECK(makeTrade() == makeTrade());
    Trade differentPrice = makeTrade();
    differentPrice.price = Price::fromTicks(1'902'601);
    CHECK(makeTrade() != differentPrice);

    CHECK(makeConnectionState() == makeConnectionState());
    ConnectionState disconnected = makeConnectionState();
    disconnected.status = ConnectionStatus::kDisconnected;
    CHECK(makeConnectionState() != disconnected);

    CHECK(makeOrderUpdate() == makeOrderUpdate());
    OrderUpdate filled = makeOrderUpdate();
    filled.status = OrderStatus::kFilled;
    CHECK(makeOrderUpdate() != filled);

    OrderUpdate differentReason = makeOrderUpdate();
    differentReason.reason = "insufficient buying power";
    CHECK(makeOrderUpdate() != differentReason);
}

TEST_CASE("raw frames preserve the payload byte for byte", "[events][raw]") {
    RawFrame frame;
    frame.sequence = SequenceNumber{3};
    frame.source = FrameSource::kMarketData;
    frame.arrival = Timestamp::fromNanos(1'700'000'000'000'000'001);
    frame.payload = std::string("[{\"T\":\"q\",\"S\":\"AAPL\"}]\n\t");

    RawFrame copy = frame;
    CHECK(frame == copy);
    CHECK(frame.payload.size() == copy.payload.size());

    copy.payload.push_back(' ');
    CHECK(frame != copy);

    RawFrame otherSource = frame;
    otherSource.source = FrameSource::kTradeUpdates;
    CHECK(frame != otherSource);
}

TEST_CASE("the event variant compares across alternatives", "[events][variant]") {
    const Event quoteEvent = makeQuote();
    const Event tradeEvent = makeTrade();

    CHECK(quoteEvent == Event{makeQuote()});
    CHECK(quoteEvent != tradeEvent);
    CHECK(std::holds_alternative<Quote>(quoteEvent));
    CHECK(std::holds_alternative<Trade>(tradeEvent));
}

TEST_CASE("variant accessors read the common fields of every alternative", "[events][variant]") {
    CHECK(vanta::sequenceOf(Event{makeQuote()}) == SequenceNumber{7});
    CHECK(vanta::sequenceOf(Event{makeTrade()}) == SequenceNumber{8});
    CHECK(vanta::sequenceOf(Event{makeConnectionState()}) == SequenceNumber{1});
    CHECK(vanta::sequenceOf(Event{makeOrderUpdate()}) == SequenceNumber{9});

    CHECK(vanta::eventTimeOf(Event{makeQuote()}) ==
          Timestamp::fromNanos(1'700'000'000'123'456'789));
    CHECK(vanta::eventTimeOf(Event{makeOrderUpdate()}) ==
          Timestamp::fromNanos(1'700'000'000'200'000'000));

    CHECK(vanta::typeNameOf(Event{makeQuote()}) == "quote");
    CHECK(vanta::typeNameOf(Event{makeTrade()}) == "trade");
    CHECK(vanta::typeNameOf(Event{makeConnectionState()}) == "connection");
    CHECK(vanta::typeNameOf(Event{makeOrderUpdate()}) == "order_update");
}

TEST_CASE("enum descriptions are stable strings", "[events][describe]") {
    CHECK(vanta::describe(FrameSource::kMarketData) == "market_data");
    CHECK(vanta::describe(FrameSource::kTradeUpdates) == "trade_updates");
    CHECK(vanta::describe(ConnectionStatus::kDisconnected) == "disconnected");
    CHECK(vanta::describe(ConnectionStatus::kSubscribed) == "subscribed");
    CHECK(vanta::describe(OrderStatus::kPendingNew) == "pending_new");
    CHECK(vanta::describe(OrderStatus::kPartiallyFilled) == "partially_filled");
    CHECK(vanta::describe(OrderStatus::kRejected) == "rejected");
}
