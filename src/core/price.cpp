#include "vanta/core/price.hpp"

#include <cstddef>
#include <limits>

namespace vanta {

namespace {

using PriceResult = Result<Price, PriceError>;

constexpr std::size_t kDecimals = static_cast<std::size_t>(Price::kDecimalPlaces);

/// Largest magnitude a positive int64 can hold: 9,223,372,036,854,775,807.
constexpr std::uint64_t kPositiveLimit =
    static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());

/// Largest magnitude a negative int64 can hold: 9,223,372,036,854,775,808.
/// One larger than the positive limit, which is why the two are tracked apart.
constexpr std::uint64_t kNegativeLimit = kPositiveLimit + 1;

constexpr bool isDigit(char c) noexcept {
    return c >= '0' && c <= '9';
}

}  // namespace

std::string_view describe(PriceError error) noexcept {
    switch (error) {
        case PriceError::kEmptyInput:
            return "empty input";
        case PriceError::kInvalidCharacter:
            return "invalid character";
        case PriceError::kMultipleDecimalPoints:
            return "multiple decimal points";
        case PriceError::kMissingDigits:
            return "missing digits";
        case PriceError::kTooManyDecimalPlaces:
            return "too many decimal places";
        case PriceError::kNegativeNotAllowed:
            return "negative value not allowed";
        case PriceError::kOverflow:
            return "value out of int64 range";
    }
    return "unknown price error";
}

PriceResult Price::parse(std::string_view text) {
    if (text.empty()) {
        return PriceResult::err(PriceError::kEmptyInput);
    }

    std::size_t index = 0;
    bool negative = false;
    if (text[index] == '+' || text[index] == '-') {
        negative = text[index] == '-';
        ++index;
    }

    // The magnitude is accumulated in an unsigned integer so that the most
    // negative representable value has somewhere to live: its magnitude is one
    // larger than int64's positive maximum.
    const std::uint64_t limit = negative ? kNegativeLimit : kPositiveLimit;

    std::uint64_t magnitude = 0;
    std::size_t integerDigits = 0;
    std::size_t fractionDigits = 0;
    bool seenPoint = false;

    for (; index < text.size(); ++index) {
        const char c = text[index];

        if (c == '.') {
            if (seenPoint) {
                return PriceResult::err(PriceError::kMultipleDecimalPoints);
            }
            seenPoint = true;
            continue;
        }

        if (!isDigit(c)) {
            return PriceResult::err(PriceError::kInvalidCharacter);
        }

        if (seenPoint) {
            ++fractionDigits;
            if (fractionDigits > kDecimals) {
                return PriceResult::err(PriceError::kTooManyDecimalPlaces);
            }
        } else {
            ++integerDigits;
        }

        const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
        if (magnitude > (limit - digit) / 10) {
            return PriceResult::err(PriceError::kOverflow);
        }
        magnitude = magnitude * 10 + digit;
    }

    if (integerDigits == 0) {
        return PriceResult::err(PriceError::kMissingDigits);
    }
    if (seenPoint && fractionDigits == 0) {
        return PriceResult::err(PriceError::kMissingDigits);
    }

    // Scale whatever fractional digits were supplied up to the full tick scale.
    for (std::size_t i = fractionDigits; i < kDecimals; ++i) {
        if (magnitude > limit / 10) {
            return PriceResult::err(PriceError::kOverflow);
        }
        magnitude *= 10;
    }

    // C++20 fixes signed integers as two's complement, so the unsigned-to-signed
    // conversion below is well defined for every magnitude within the limit.
    const std::int64_t ticks = negative ? static_cast<std::int64_t>(0ULL - magnitude)
                                        : static_cast<std::int64_t>(magnitude);

    return PriceResult::ok(Price::fromTicks(ticks));
}

PriceResult Price::parseNonNegative(std::string_view text) {
    PriceResult parsed = parse(text);
    if (!parsed) {
        return parsed;
    }
    // Checked on the text rather than the value so that "-0.0000" is rejected
    // too: a feed that sends a negative price is malformed regardless of whether
    // the value happens to round to zero.
    if (text.front() == '-') {
        return PriceResult::err(PriceError::kNegativeNotAllowed);
    }
    return parsed;
}

std::string Price::toString() const {
    const bool negative = ticks_ < 0;
    const std::uint64_t magnitude =
        negative ? 0ULL - static_cast<std::uint64_t>(ticks_) : static_cast<std::uint64_t>(ticks_);

    const std::uint64_t scale = static_cast<std::uint64_t>(kScale);
    const std::uint64_t whole = magnitude / scale;
    const std::uint64_t fraction = magnitude % scale;

    std::string out;
    if (negative) {
        out.push_back('-');
    }
    out += std::to_string(whole);
    out.push_back('.');

    const std::string fractionDigits = std::to_string(fraction);
    out.append(kDecimals - fractionDigits.size(), '0');
    out += fractionDigits;
    return out;
}

PriceResult Price::checkedAdd(Price lhs, Price rhs) noexcept {
    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
    constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();

    const std::int64_t a = lhs.ticks_;
    const std::int64_t b = rhs.ticks_;

    if (b > 0 && a > kMax - b) {
        return PriceResult::err(PriceError::kOverflow);
    }
    if (b < 0 && a < kMin - b) {
        return PriceResult::err(PriceError::kOverflow);
    }
    return PriceResult::ok(Price::fromTicks(a + b));
}

PriceResult Price::checkedSub(Price lhs, Price rhs) noexcept {
    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
    constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();

    const std::int64_t a = lhs.ticks_;
    const std::int64_t b = rhs.ticks_;

    if (b < 0 && a > kMax + b) {
        return PriceResult::err(PriceError::kOverflow);
    }
    if (b > 0 && a < kMin + b) {
        return PriceResult::err(PriceError::kOverflow);
    }
    return PriceResult::ok(Price::fromTicks(a - b));
}

}  // namespace vanta
