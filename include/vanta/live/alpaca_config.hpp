#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "vanta/core/result.hpp"

namespace vanta {

struct AlpacaConfig {
    std::string feed_url;
    std::string key_id;
    std::string secret_key;
    std::string feed;
    std::string symbol;
};

using EnvironmentLookup = std::function<std::optional<std::string>(std::string_view variable)>;

/// Loads the recorder's complete environment configuration. Error text may
/// name a variable, but never includes environment values.
[[nodiscard]] Result<AlpacaConfig, std::string> loadAlpacaConfig(const EnvironmentLookup& lookup);

[[nodiscard]] Result<AlpacaConfig, std::string> loadAlpacaConfigFromEnvironment();

}  // namespace vanta
