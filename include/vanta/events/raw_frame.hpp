#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "vanta/core/types.hpp"

namespace vanta {

/// Which external connection a frame arrived on.
enum class FrameSource : std::uint8_t {
    kMarketData = 0,
    kTradeUpdates = 1,
};

[[nodiscard]] std::string_view describe(FrameSource source) noexcept;

/// One message exactly as it came off the wire, plus the metadata Vanta
/// attaches on arrival.
///
/// The payload is stored byte-for-byte and is never reformatted, pretty-printed,
/// or re-encoded. raw.ndjson is the source of truth for replay, so anything that
/// rewrites a payload destroys the ability to reproduce a session.
struct RawFrame {
    SequenceNumber sequence{};
    FrameSource source{FrameSource::kMarketData};
    Timestamp arrival{};
    std::string payload;

    friend bool operator==(const RawFrame&, const RawFrame&) = default;
};

}  // namespace vanta
