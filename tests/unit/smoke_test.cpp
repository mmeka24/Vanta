#include <catch2/catch_test_macros.hpp>

#include <string>

#include "vanta/version.hpp"

TEST_CASE("version header reports a non-empty semantic version", "[smoke][version]") {
    REQUIRE_FALSE(vanta::kVersion.empty());

    // Hand-written expectation: the string form must be exactly the three
    // numeric components joined by dots, so a drift between the configured
    // string and the numeric macros fails here rather than in a later log.
    const std::string expected = std::to_string(vanta::kVersionMajor) + "." +
                                 std::to_string(vanta::kVersionMinor) + "." +
                                 std::to_string(vanta::kVersionPatch);
    REQUIRE(std::string{vanta::kVersion} == expected);
}
