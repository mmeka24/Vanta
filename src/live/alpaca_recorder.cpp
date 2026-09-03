#include "vanta/live/alpaca_recorder.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <utility>

#include "vanta/events/serialize.hpp"
#include "vanta/live/alpaca_protocol.hpp"

namespace vanta {

ReconnectBackoff::ReconnectBackoff(std::chrono::milliseconds initial,
                                   std::chrono::milliseconds maximum)
    : initial_(initial), maximum_(maximum), current_(initial) {
    if (initial_.count() <= 0 || maximum_ < initial_) {
        throw std::invalid_argument("reconnect delay must be positive and not exceed maximum");
    }
}

std::chrono::milliseconds ReconnectBackoff::next() {
    const auto result = current_;
    if (current_ < maximum_) {
        current_ = std::min(maximum_, current_ * 2);
    }
    return result;
}

void ReconnectBackoff::reset() noexcept {
    current_ = initial_;
}

AlpacaRecorderClient::AlpacaRecorderClient(AlpacaConfig config,
                                           const std::filesystem::path& output_path,
                                           WebSocketFactory socket_factory,
                                           AlpacaRecorderOptions options)
    : config_(std::move(config)),
      socket_factory_(std::move(socket_factory)),
      options_(options),
      queue_(options_.queue_capacity),
      recorder_(output_path) {
    if (!socket_factory_) {
        throw std::invalid_argument("WebSocket factory is required");
    }
}

bool AlpacaRecorderClient::isReady() const {
    return recorder_.isOpen();
}

Timestamp AlpacaRecorderClient::now() {
    const auto time = std::chrono::system_clock::now().time_since_epoch();
    return Timestamp::fromNanos(std::chrono::duration_cast<std::chrono::nanoseconds>(time).count());
}

bool AlpacaRecorderClient::enqueuePayload(std::string payload) {
    std::lock_guard<std::mutex> lock(ingest_mutex_);
    RawFrame frame{next_sequence_++, FrameSource::kMarketData, now(), std::move(payload)};
    return queue_.push(std::move(frame)) == PushStatus::kAccepted;
}

bool AlpacaRecorderClient::enqueueMarker(std::string_view status, std::string_view reason) {
    const std::string marker = "{\"T\":\"vanta_status\",\"status\":\"" + escapeJson(status) +
                               "\",\"reason\":\"" + escapeJson(reason) + "\"}";
    return enqueuePayload(marker);
}

void AlpacaRecorderClient::handleSocketEvent(IWebSocket& socket, const WebSocketEvent& event) {
    switch (event.type) {
        case WebSocketEventType::kOpen: {
            spdlog::info("Alpaca market-data socket connected");
            if (!socket.sendText(makeAlpacaAuthMessage(config_))) {
                spdlog::error("failed to send Alpaca authentication request");
                std::lock_guard<std::mutex> lock(state_mutex_);
                disconnected_ = true;
                state_changed_.notify_all();
            }
            break;
        }
        case WebSocketEventType::kMessage: {
            const auto classification = classifyAlpacaMessage(event.text, config_.symbol);
            if (classification.kind == AlpacaMessageKind::kAuthenticated) {
                {
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    authenticated_ = true;
                }
                spdlog::info("Alpaca market-data authentication succeeded");
                if (!socket.sendText(makeAlpacaSubscriptionMessage(config_))) {
                    spdlog::error("failed to send Alpaca subscription request");
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    disconnected_ = true;
                    state_changed_.notify_all();
                }
            } else if (classification.kind == AlpacaMessageKind::kSubscribed) {
                spdlog::info("subscribed to AAPL quotes and trades");
            } else if (classification.kind == AlpacaMessageKind::kMarketData) {
                if (!enqueuePayload(event.text)) {
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    fatal_error_ = true;
                    state_changed_.notify_all();
                }
            } else if (classification.kind == AlpacaMessageKind::kError ||
                       classification.kind == AlpacaMessageKind::kMalformed) {
                spdlog::error("market-data protocol failure: {}", classification.detail);
                std::lock_guard<std::mutex> lock(state_mutex_);
                disconnected_ = true;
                state_changed_.notify_all();
            }
            break;
        }
        case WebSocketEventType::kError:
            spdlog::error("Alpaca market-data socket error: {}", event.text);
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                disconnected_ = true;
            }
            state_changed_.notify_all();
            break;
        case WebSocketEventType::kClosed:
            spdlog::warn("Alpaca market-data socket disconnected: {}", event.text);
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                disconnected_ = true;
            }
            state_changed_.notify_all();
            break;
    }
}

bool AlpacaRecorderClient::run(const std::atomic<bool>& stop_requested) {
    if (!recorder_.isOpen()) {
        spdlog::error("could not open raw market-data output file");
        return false;
    }

    std::thread writer([this] {
        while (auto frame = queue_.pop()) {
            if (!recorder_.record(*frame)) {
                spdlog::error("raw market-data write failed; stopping recorder");
                {
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    fatal_error_ = true;
                }
                queue_.close();
                state_changed_.notify_all();
                return;
            }
        }
    });

    ReconnectBackoff backoff(options_.initial_reconnect_delay, options_.maximum_reconnect_delay);
    bool reconnecting = false;

    while (!stop_requested.load()) {
        auto socket = socket_factory_();
        if (!socket) {
            spdlog::error("WebSocket factory returned no client");
            std::lock_guard<std::mutex> lock(state_mutex_);
            fatal_error_ = true;
            break;
        }

        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            disconnected_ = false;
            authenticated_ = false;
        }

        if (reconnecting) {
            if (!enqueueMarker("reconnecting", "starting_new_connection")) {
                break;
            }
            spdlog::info("reconnecting to Alpaca market data");
        }

        socket->setHandler([this, pointer = socket.get()](const WebSocketEvent& event) {
            handleSocketEvent(*pointer, event);
        });
        socket->start(config_.feed_url);

        {
            std::unique_lock<std::mutex> lock(state_mutex_);
            while (!disconnected_ && !fatal_error_ && !stop_requested.load()) {
                state_changed_.wait_for(lock, std::chrono::milliseconds(100));
            }
        }

        socket->stop();

        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (fatal_error_) {
                break;
            }
        }
        if (stop_requested.load()) {
            break;
        }

        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (authenticated_) {
                backoff.reset();
            }
        }
        if (!enqueueMarker("gap", "market_data_disconnected")) {
            break;
        }
        reconnecting = true;
        const auto delay = backoff.next();
        spdlog::warn("market data unavailable; reconnecting in {} ms", delay.count());

        auto waited = std::chrono::milliseconds{0};
        while (waited < delay && !stop_requested.load()) {
            const auto slice = std::min(std::chrono::milliseconds{100}, delay - waited);
            std::this_thread::sleep_for(slice);
            waited += slice;
        }
    }

    queue_.close();
    writer.join();
    recorder_.close();
    spdlog::info("market-data recorder stopped cleanly");

    std::lock_guard<std::mutex> lock(state_mutex_);
    return !fatal_error_;
}

}  // namespace vanta
