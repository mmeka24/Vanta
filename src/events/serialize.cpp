#include "vanta/events/serialize.hpp"

#include <variant>

namespace vanta {

namespace {

constexpr char kHexDigits[] = "0123456789abcdef";

void appendQuoted(std::string& out, std::string_view text) {
    out.push_back('"');
    out += escapeJson(text);
    out.push_back('"');
}

void appendField(std::string& out, std::string_view name, std::string_view value) {
    appendQuoted(out, name);
    out.push_back(':');
    appendQuoted(out, value);
}

void appendField(std::string& out, std::string_view name, std::uint64_t value) {
    appendQuoted(out, name);
    out.push_back(':');
    out += std::to_string(value);
}

void appendField(std::string& out, std::string_view name, std::int64_t value) {
    appendQuoted(out, name);
    out.push_back(':');
    out += std::to_string(value);
}

}  // namespace

std::string escapeJson(std::string_view text) {
    std::string out;
    out.reserve(text.size());

    for (const char c : text) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default: {
                const unsigned char byte = static_cast<unsigned char>(c);
                if (byte < 0x20) {
                    out += "\\u00";
                    out.push_back(kHexDigits[(byte >> 4) & 0x0F]);
                    out.push_back(kHexDigits[byte & 0x0F]);
                } else {
                    out.push_back(c);
                }
                break;
            }
        }
    }

    return out;
}

std::string toJson(const Quote& quote) {
    std::string out = "{";
    appendField(out, "type", "quote");
    out.push_back(',');
    appendField(out, "seq", quote.sequence.value());
    out.push_back(',');
    appendField(out, "event_time_ns", quote.event_time.nanos());
    out.push_back(',');
    appendField(out, "symbol", quote.symbol);
    out.push_back(',');
    appendField(out, "bid", quote.bid.toString());
    out.push_back(',');
    appendField(out, "ask", quote.ask.toString());
    out.push_back(',');
    appendField(out, "bid_size", quote.bid_size);
    out.push_back(',');
    appendField(out, "ask_size", quote.ask_size);
    out.push_back('}');
    return out;
}

std::string toJson(const Trade& trade) {
    std::string out = "{";
    appendField(out, "type", "trade");
    out.push_back(',');
    appendField(out, "seq", trade.sequence.value());
    out.push_back(',');
    appendField(out, "event_time_ns", trade.event_time.nanos());
    out.push_back(',');
    appendField(out, "symbol", trade.symbol);
    out.push_back(',');
    appendField(out, "price", trade.price.toString());
    out.push_back(',');
    appendField(out, "size", trade.size);
    out.push_back('}');
    return out;
}

std::string toJson(const ConnectionState& state) {
    std::string out = "{";
    appendField(out, "type", "connection");
    out.push_back(',');
    appendField(out, "seq", state.sequence.value());
    out.push_back(',');
    appendField(out, "event_time_ns", state.event_time.nanos());
    out.push_back(',');
    appendField(out, "status", describe(state.status));
    out.push_back(',');
    appendField(out, "detail", state.detail);
    out.push_back('}');
    return out;
}

std::string toJson(const OrderUpdate& update) {
    std::string out = "{";
    appendField(out, "type", "order_update");
    out.push_back(',');
    appendField(out, "seq", update.sequence.value());
    out.push_back(',');
    appendField(out, "event_time_ns", update.event_time.nanos());
    out.push_back(',');
    appendField(out, "client_order_id", update.client_order_id);
    out.push_back(',');
    appendField(out, "broker_order_id", update.broker_order_id);
    out.push_back(',');
    appendField(out, "status", describe(update.status));
    out.push_back(',');
    appendField(out, "filled_quantity", update.filled_quantity);
    out.push_back(',');
    appendField(out, "average_fill_price", update.average_fill_price.toString());
    out.push_back(',');
    appendField(out, "reason", update.reason);
    out.push_back('}');
    return out;
}

std::string toJson(const Event& event) {
    return std::visit([](const auto& typed) { return toJson(typed); }, event);
}

std::string toJson(const RawFrame& frame) {
    std::string out = "{";
    appendField(out, "seq", frame.sequence.value());
    out.push_back(',');
    appendField(out, "source", describe(frame.source));
    out.push_back(',');
    appendField(out, "arrival_ns", frame.arrival.nanos());
    out.push_back(',');
    appendField(out, "payload", frame.payload);
    out.push_back('}');
    return out;
}

}  // namespace vanta
