#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>

namespace vanta {

/// Snapshot of how full the queue is and how much it has carried.
///
/// high_water_depth is the value that matters operationally: it shows whether a
/// burst ever came close to the capacity limit, which a spot reading of depth
/// would miss.
struct QueueMetrics {
    std::size_t depth{0};
    std::size_t high_water_depth{0};
    std::uint64_t accepted_total{0};
    std::uint64_t rejected_after_close{0};
};

enum class PushStatus : std::uint8_t {
    kAccepted,
    kClosed,
};

/// Fixed-capacity queue that transfers items from producer threads to a single
/// consumer.
///
/// The capacity is fixed on purpose. An unbounded queue turns a slow consumer
/// into silent memory growth and ever-staler data; a bounded one makes the
/// producer block, which is visible and measurable.
///
/// Close semantics are explicit, because shutdown is where queues lose data:
///   - push on a closed queue never stores the item and returns kClosed;
///   - pop keeps returning already-accepted items after close, and only returns
///     nullopt once the queue is both closed and empty;
///   - close wakes every blocked thread and is safe to call more than once.
template <class T>
class BoundedQueue {
  public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {
        if (capacity_ == 0) {
            throw std::invalid_argument("BoundedQueue capacity must be greater than zero");
        }
    }

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;
    BoundedQueue(BoundedQueue&&) = delete;
    BoundedQueue& operator=(BoundedQueue&&) = delete;

    ~BoundedQueue() = default;

    /// Blocks while the queue is full. Returns kClosed if the queue was closed
    /// before the item could be accepted; the item is discarded in that case.
    [[nodiscard]] PushStatus push(T value) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [this] { return closed_ || items_.size() < capacity_; });

        if (closed_) {
            ++rejected_after_close_;
            return PushStatus::kClosed;
        }

        items_.push(std::move(value));
        ++accepted_total_;
        if (items_.size() > high_water_depth_) {
            high_water_depth_ = items_.size();
        }

        lock.unlock();
        not_empty_.notify_one();
        return PushStatus::kAccepted;
    }

    /// Blocks until an item is available. Returns nullopt only when the queue is
    /// closed and every accepted item has been handed out.
    [[nodiscard]] std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] { return closed_ || !items_.empty(); });

        if (items_.empty()) {
            return std::nullopt;
        }

        T value = std::move(items_.front());
        items_.pop();

        lock.unlock();
        not_full_.notify_one();
        return value;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (closed_) {
                return;
            }
            closed_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    [[nodiscard]] bool closed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

    [[nodiscard]] std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return items_.size();
    }

    [[nodiscard]] std::size_t capacity() const noexcept {
        return capacity_;
    }

    [[nodiscard]] QueueMetrics metrics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return QueueMetrics{items_.size(), high_water_depth_, accepted_total_,
                            rejected_after_close_};
    }

  private:
    const std::size_t capacity_;

    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;

    std::queue<T> items_;
    bool closed_{false};
    std::size_t high_water_depth_{0};
    std::uint64_t accepted_total_{0};
    std::uint64_t rejected_after_close_{0};
};

}  // namespace vanta
