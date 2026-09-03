#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

#include "vanta/core/price.hpp"

using vanta::Price;
using vanta::PriceError;

namespace {

/// Expected ticks are written out by hand in every test below. The scale is
/// fixed at 1 tick = 0.0001 USD, so "123.45" must be 1234500 ticks and nothing
/// else. Deriving the expectation from the implementation would prove nothing.
std::int64_t parsedTicks(std::string_view text) {
    const auto result = Price::parse(text);
    REQUIRE(result.hasValue());
    return result.value().ticks();
}

PriceError parseError(std::string_view text) {
    const auto result = Price::parse(text);
    REQUIRE_FALSE(result.hasValue());
    return result.error();
}

}  // namespace

TEST_CASE("valid positive prices parse to hand-calculated ticks", "[price][parse]") {
    CHECK(parsedTicks("123.45") == 1'234'500);
    CHECK(parsedTicks("0.0001") == 1);
    CHECK(parsedTicks("100") == 1'000'000);
    CHECK(parsedTicks("0") == 0);
    CHECK(parsedTicks("190.2500") == 1'902'500);
    CHECK(parsedTicks("+1.5") == 15'000);
    CHECK(parsedTicks("000123.4500") == 1'234'500);
}

TEST_CASE("formatting produces canonical four-decimal output", "[price][format]") {
    CHECK(Price::fromTicks(1'234'500).toString() == "123.4500");
    CHECK(Price::fromTicks(1).toString() == "0.0001");
    CHECK(Price::fromTicks(0).toString() == "0.0000");
    CHECK(Price::fromTicks(-500).toString() == "-0.0500");
    CHECK(Price::fromTicks(-1).toString() == "-0.0001");
    CHECK(Price::fromTicks(1'000'000).toString() == "100.0000");

    // The extreme values must format without wrapping their magnitude.
    CHECK(Price::fromTicks(std::numeric_limits<std::int64_t>::max()).toString() ==
          "922337203685477.5807");
    CHECK(Price::fromTicks(std::numeric_limits<std::int64_t>::min()).toString() ==
          "-922337203685477.5808");
}

TEST_CASE("parse and format round-trip exactly", "[price][parse][format]") {
    const std::int64_t samples[] = {0,
                                    1,
                                    -1,
                                    9'999,
                                    -9'999,
                                    1'234'500,
                                    -1'234'500,
                                    std::numeric_limits<std::int64_t>::max(),
                                    std::numeric_limits<std::int64_t>::min()};

    for (const std::int64_t ticks : samples) {
        const Price original = Price::fromTicks(ticks);
        const auto reparsed = Price::parse(original.toString());
        REQUIRE(reparsed.hasValue());
        CHECK(reparsed.value() == original);
    }
}

TEST_CASE("one tick is the smallest representable increment", "[price][precision]") {
    CHECK(parsedTicks("0.0001") == 1);
    CHECK(parsedTicks("0.0002") - parsedTicks("0.0001") == 1);

    // Half a tick has no exact representation and must be rejected rather than
    // rounded to zero or to one tick.
    CHECK(parseError("0.00005") == PriceError::kTooManyDecimalPlaces);
}

TEST_CASE("too many decimal places is an error", "[price][parse]") {
    CHECK(parseError("1.23456") == PriceError::kTooManyDecimalPlaces);
    CHECK(parseError("0.000000") == PriceError::kTooManyDecimalPlaces);
    CHECK(parseError("-1.00001") == PriceError::kTooManyDecimalPlaces);
}

TEST_CASE("malformed input is rejected with a specific error", "[price][parse]") {
    CHECK(parseError("") == PriceError::kEmptyInput);
    CHECK(parseError("abc") == PriceError::kInvalidCharacter);
    CHECK(parseError("1 .5") == PriceError::kInvalidCharacter);
    CHECK(parseError(" 1.5") == PriceError::kInvalidCharacter);
    CHECK(parseError("1.5 ") == PriceError::kInvalidCharacter);
    CHECK(parseError("1e5") == PriceError::kInvalidCharacter);
    CHECK(parseError("1..5") == PriceError::kMultipleDecimalPoints);
    CHECK(parseError("1.2.3") == PriceError::kMultipleDecimalPoints);
    CHECK(parseError(".5") == PriceError::kMissingDigits);
    CHECK(parseError("1.") == PriceError::kMissingDigits);
    CHECK(parseError("-") == PriceError::kMissingDigits);
}

TEST_CASE("negative values are accepted by parse and refused by parseNonNegative",
          "[price][parse][sign]") {
    CHECK(parsedTicks("-1.5") == -15'000);
    CHECK(parsedTicks("-0.0001") == -1);

    const auto rejected = Price::parseNonNegative("-1.0000");
    REQUIRE_FALSE(rejected.hasValue());
    CHECK(rejected.error() == PriceError::kNegativeNotAllowed);

    // Negative zero is malformed input from a feed even though its value is
    // zero, so it is refused too.
    const auto negativeZero = Price::parseNonNegative("-0.0000");
    REQUIRE_FALSE(negativeZero.hasValue());
    CHECK(negativeZero.error() == PriceError::kNegativeNotAllowed);

    const auto accepted = Price::parseNonNegative("190.2500");
    REQUIRE(accepted.hasValue());
    CHECK(accepted.value().ticks() == 1'902'500);

    // A malformed non-negative string reports its own error, not the sign error.
    const auto malformed = Price::parseNonNegative("1.23456");
    REQUIRE_FALSE(malformed.hasValue());
    CHECK(malformed.error() == PriceError::kTooManyDecimalPlaces);
}

TEST_CASE("int64 boundaries are exact and one step beyond overflows", "[price][overflow]") {
    // int64 max is 9223372036854775807 ticks, which is 922337203685477.5807 USD.
    const auto maximum = Price::parse("922337203685477.5807");
    REQUIRE(maximum.hasValue());
    CHECK(maximum.value().ticks() == std::numeric_limits<std::int64_t>::max());

    CHECK(parseError("922337203685477.5808") == PriceError::kOverflow);

    // int64 min is -9223372036854775808 ticks. Its magnitude is one larger than
    // the positive maximum, so it must be representable while its successor is
    // not.
    const auto minimum = Price::parse("-922337203685477.5808");
    REQUIRE(minimum.hasValue());
    CHECK(minimum.value().ticks() == std::numeric_limits<std::int64_t>::min());

    CHECK(parseError("-922337203685477.5809") == PriceError::kOverflow);

    // Overflow that only appears when the fractional digits are scaled up:
    // 1e15 fits in int64, but 1e15 * 10000 does not.
    CHECK(parseError("1000000000000000") == PriceError::kOverflow);

    CHECK(parseError("99999999999999999999") == PriceError::kOverflow);
}

TEST_CASE("checked arithmetic reports overflow instead of wrapping", "[price][overflow]") {
    const Price max = Price::fromTicks(std::numeric_limits<std::int64_t>::max());
    const Price min = Price::fromTicks(std::numeric_limits<std::int64_t>::min());
    const Price oneTick = Price::fromTicks(1);

    const auto sum = Price::checkedAdd(Price::fromTicks(5), Price::fromTicks(7));
    REQUIRE(sum.hasValue());
    CHECK(sum.value().ticks() == 12);

    const auto difference =
        Price::checkedSub(Price::fromTicks(1'902'700), Price::fromTicks(1'902'500));
    REQUIRE(difference.hasValue());
    CHECK(difference.value().ticks() == 200);

    CHECK_FALSE(Price::checkedAdd(max, oneTick).hasValue());
    CHECK(Price::checkedAdd(max, oneTick).error() == PriceError::kOverflow);
    CHECK_FALSE(Price::checkedSub(min, oneTick).hasValue());
    CHECK(Price::checkedSub(min, oneTick).error() == PriceError::kOverflow);

    // Operations that stay in range must still succeed at the boundary.
    const auto atMax = Price::checkedAdd(max, Price::fromTicks(0));
    REQUIRE(atMax.hasValue());
    CHECK(atMax.value() == max);
}

TEST_CASE("prices order by tick value", "[price][compare]") {
    CHECK(Price::fromTicks(1) < Price::fromTicks(2));
    CHECK(Price::fromTicks(-1) < Price::fromTicks(0));
    CHECK(Price::fromTicks(1'234'500) == Price::fromTicks(1'234'500));
    CHECK(Price::fromTicks(1) != Price::fromTicks(-1));
}

TEST_CASE("every error code has a distinct description", "[price][error]") {
    CHECK(vanta::describe(PriceError::kEmptyInput) == "empty input");
    CHECK(vanta::describe(PriceError::kOverflow) == "value out of int64 range");
    CHECK(vanta::describe(PriceError::kTooManyDecimalPlaces) == "too many decimal places");
    CHECK(vanta::describe(PriceError::kNegativeNotAllowed) == "negative value not allowed");
}
