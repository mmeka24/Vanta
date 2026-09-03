#include <atomic>
#include <csignal>
#include <filesystem>
#include <iostream>

#include <spdlog/spdlog.h>

#include "vanta/live/alpaca_config.hpp"
#include "vanta/live/alpaca_recorder.hpp"
#include "vanta/live/websocket.hpp"

namespace {

std::atomic<bool> g_stop_requested{false};

extern "C" void requestStop(int) {
    g_stop_requested.store(true);
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc > 2) {
        std::cerr << "usage: vanta_recorder [raw-output.ndjson]\n";
        return 2;
    }

    const auto config = vanta::loadAlpacaConfigFromEnvironment();
    if (!config) {
        spdlog::error("configuration error: {}", config.error());
        return 2;
    }

    const std::filesystem::path output_path = argc == 2 ? argv[1] : "data/raw.ndjson";
    vanta::AlpacaRecorderClient recorder(config.value(), output_path, vanta::makeIxWebSocket);
    if (!recorder.isReady()) {
        spdlog::error("recorder output is unavailable: {}", output_path.string());
        return 1;
    }

    std::signal(SIGINT, requestStop);
    spdlog::info("starting AAPL market-data recorder; press Ctrl-C to stop");
    return recorder.run(g_stop_requested) ? 0 : 1;
}
