#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>

#include "vanta/core/bounded_queue.hpp"
#include "vanta/events/raw_frame.hpp"
#include "vanta/live/alpaca_config.hpp"
#include "vanta/live/websocket.hpp"
#include "vanta/record/raw_recorder.hpp"

namespace vanta {

class ReconnectBackoff {
  public:
    ReconnectBackoff(std::chrono::milliseconds initial, std::chrono::milliseconds maximum);

    [[nodiscard]] std::chrono::milliseconds next();
    void reset() noexcept;

  private:
    std::chrono::milliseconds initial_;
    std::chrono::milliseconds maximum_;
    std::chrono::milliseconds current_;
};

struct AlpacaRecorderOptions {
    std::size_t queue_capacity{1024};
    std::chrono::milliseconds initial_reconnect_delay{1000};
    std::chrono::milliseconds maximum_reconnect_delay{30000};
};

/// Recorder-only live boundary. It authenticates and receives market data but
/// contains no trading, strategy, parsing, or order-submission path.
class AlpacaRecorderClient {
  public:
    AlpacaRecorderClient(AlpacaConfig config, const std::filesystem::path& output_path,
                         WebSocketFactory socket_factory, AlpacaRecorderOptions options = {});

    AlpacaRecorderClient(const AlpacaRecorderClient&) = delete;
    AlpacaRecorderClient& operator=(const AlpacaRecorderClient&) = delete;

    /// Runs until stop_requested becomes true or a fatal recorder error occurs.
    /// Returns false when startup or recording failed.
    [[nodiscard]] bool run(const std::atomic<bool>& stop_requested);

    [[nodiscard]] bool isReady() const;

  private:
    void handleSocketEvent(IWebSocket& socket, const WebSocketEvent& event);
    [[nodiscard]] bool enqueuePayload(std::string payload);
    [[nodiscard]] bool enqueueMarker(std::string_view status, std::string_view reason);
    [[nodiscard]] static Timestamp now();

    AlpacaConfig config_;
    WebSocketFactory socket_factory_;
    AlpacaRecorderOptions options_;
    BoundedQueue<RawFrame> queue_;
    RawRecorder recorder_;

    mutable std::mutex state_mutex_;
    std::condition_variable state_changed_;
    bool disconnected_{false};
    bool fatal_error_{false};
    bool authenticated_{false};

    std::mutex ingest_mutex_;
    SequenceNumber next_sequence_{1};
};

}  // namespace vanta
