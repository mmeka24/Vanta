#pragma once

#include <compare>
#include <cstdint>
#include <string>
#include <string_view>

#include "vanta/core/result.hpp"

namespace vanta {

enum class PriceError : std::uint8_t {
    kEmptyInput,
    kInvalidCharacter,
    kMultipleDecimalPoints,
    kMissingDigits,
    kTooManyDecimalPlaces,
    kNegativeNotAllowed,
    kOverflow,
};

/// Stable, human-readable name for logs and test failure messages.
[[nodiscard]] std::string_view describe(PriceError error) noexcept;

/// Fixed-point money value.
///
/// One tick is 0.0001 USD, so the value is stored as an int64_t count of ticks.
/// No operation in this class uses float or double: floating point cannot
/// represent decimal prices exactly, and two runs that round differently would
/// break the determinism the whole engine depends on.
///
/// The representable range is roughly +/- 922,337,203,685,477.5807 USD.
class Price {
  public:
    /// Ticks per USD. 1 tick = 0.0001 USD.
    static constexpr std::int64_t kScale = 10'000;

    /// Number of digits after the decimal point in the canonical form.
    static constexpr int kDecimalPlaces = 4;

    constexpr Price() noexcept = default;

    [[nodiscard]] static constexpr Price fromTicks(std::int64_t ticks) noexcept {
        return Price(ticks);
    }

    [[nodiscard]] constexpr std::int64_t ticks() const noexcept {
        return ticks_;
    }

    /// Parse a decimal string such as "123.45" or "-0.0001".
    ///
    /// Accepts an optional leading '+' or '-', at least one integer digit, and
    /// at most kDecimalPlaces fractional digits. Everything else - whitespace,
    /// exponents, a bare ".5", a trailing "1.", or a value outside int64 range -
    /// returns an error. Nothing is rounded and nothing is truncated.
    [[nodiscard]] static Result<Price, PriceError> parse(std::string_view text);

    /// Same as parse(), but a leading '-' is rejected outright. Quote and trade
    /// prices use this; signed values such as P&L use parse().
    [[nodiscard]] static Result<Price, PriceError> parseNonNegative(std::string_view text);

    /// Canonical decimal form: always exactly kDecimalPlaces fractional digits,
    /// e.g. Price::fromTicks(1) -> "0.0001". parse(p.toString()) == p for every
    /// representable p.
    [[nodiscard]] std::string toString() const;

    /// Addition and subtraction that report int64 overflow instead of wrapping.
    [[nodiscard]] static Result<Price, PriceError> checkedAdd(Price lhs, Price rhs) noexcept;
    [[nodiscard]] static Result<Price, PriceError> checkedSub(Price lhs, Price rhs) noexcept;

    friend constexpr bool operator==(Price, Price) noexcept = default;
    friend constexpr auto operator<=>(Price, Price) noexcept = default;

  private:
    constexpr explicit Price(std::int64_t ticks) noexcept : ticks_(ticks) {}

    std::int64_t ticks_{0};
};

}  // namespace vanta
