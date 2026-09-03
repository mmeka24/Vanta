#pragma once

#include <chrono>
#include <compare>
#include <cstdint>
#include <string>

namespace vanta {

/// Share counts. Sizes reported by an exchange are never negative; signed
/// positions arrive with the portfolio in a later chunk.
using Quantity = std::uint64_t;

using Symbol = std::string;

using Duration = std::chrono::nanoseconds;

/// Monotonic counter assigned by Vanta as frames arrive, not by the exchange.
///
/// It exists so a recorded session can be replayed in exactly the order it was
/// received, independent of any timestamp the venue supplied.
class SequenceNumber {
  public:
    constexpr SequenceNumber() noexcept = default;

    constexpr explicit SequenceNumber(std::uint64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr std::uint64_t value() const noexcept {
        return value_;
    }

    constexpr SequenceNumber& operator++() noexcept {
        ++value_;
        return *this;
    }

    constexpr SequenceNumber operator++(int) noexcept {
        const SequenceNumber previous = *this;
        ++value_;
        return previous;
    }

    friend constexpr bool operator==(SequenceNumber, SequenceNumber) noexcept = default;
    friend constexpr auto operator<=>(SequenceNumber, SequenceNumber) noexcept = default;

  private:
    std::uint64_t value_{0};
};

/// A point in time as nanoseconds since the Unix epoch.
///
/// Stored as a plain integer so replayed timestamps are bit-identical to the
/// recorded ones. Construction is explicit: nothing reads the system clock on
/// its own, and decision code receives time through the injected Clock added in
/// a later chunk.
class Timestamp {
  public:
    constexpr Timestamp() noexcept = default;

    [[nodiscard]] static constexpr Timestamp fromNanos(std::int64_t nanos_since_epoch) noexcept {
        return Timestamp(nanos_since_epoch);
    }

    [[nodiscard]] constexpr std::int64_t nanos() const noexcept {
        return nanos_;
    }

    /// Precondition: both timestamps are real Unix times, so the difference
    /// cannot overflow int64. Signed overflow here would be undefined behaviour
    /// and is caught by UBSan in the debug preset.
    friend constexpr Duration operator-(Timestamp lhs, Timestamp rhs) noexcept {
        return Duration(lhs.nanos_ - rhs.nanos_);
    }

    friend constexpr Timestamp operator+(Timestamp point, Duration offset) noexcept {
        return Timestamp(point.nanos_ + offset.count());
    }

    friend constexpr bool operator==(Timestamp, Timestamp) noexcept = default;
    friend constexpr auto operator<=>(Timestamp, Timestamp) noexcept = default;

  private:
    constexpr explicit Timestamp(std::int64_t nanos_since_epoch) noexcept
        : nanos_(nanos_since_epoch) {}

    std::int64_t nanos_{0};
};

}  // namespace vanta
