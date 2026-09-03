#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <numeric>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "vanta/core/bounded_queue.hpp"

using vanta::BoundedQueue;
using vanta::PushStatus;
using namespace std::chrono_literals;

// Catch2 assertion macros are not thread-safe, so worker threads only ever
// record results into atomics or thread-local vectors. Every CHECK below runs
// on the main thread after the workers have been joined.

TEST_CASE("items come out in the order they went in", "[queue][fifo]") {
    BoundedQueue<int> queue(8);

    for (int value = 1; value <= 5; ++value) {
        REQUIRE(queue.push(value) == PushStatus::kAccepted);
    }
    REQUIRE(queue.size() == 5);

    for (int expected = 1; expected <= 5; ++expected) {
        const auto value = queue.pop();
        REQUIRE(value.has_value());
        CHECK(*value == expected);
    }
    CHECK(queue.size() == 0);
}

TEST_CASE("capacity is fixed and zero capacity is rejected", "[queue][capacity]") {
    BoundedQueue<int> queue(3);
    CHECK(queue.capacity() == 3);
    CHECK_FALSE(queue.closed());
    CHECK_THROWS_AS(BoundedQueue<int>(0), std::invalid_argument);
}

TEST_CASE("a closed queue rejects pushes and drains what it already accepted", "[queue][close]") {
    BoundedQueue<int> queue(4);

    REQUIRE(queue.push(1) == PushStatus::kAccepted);
    REQUIRE(queue.push(2) == PushStatus::kAccepted);
    queue.close();

    CHECK(queue.closed());

    // Nothing new is accepted after close.
    CHECK(queue.push(3) == PushStatus::kClosed);
    CHECK(queue.size() == 2);

    // Everything accepted before close is still handed out, in order.
    const auto first = queue.pop();
    REQUIRE(first.has_value());
    CHECK(*first == 1);

    const auto second = queue.pop();
    REQUIRE(second.has_value());
    CHECK(*second == 2);

    // Only once closed and empty does pop report the end of the stream.
    CHECK_FALSE(queue.pop().has_value());
    CHECK_FALSE(queue.pop().has_value());

    // close is idempotent.
    queue.close();
    CHECK(queue.closed());

    const auto metrics = queue.metrics();
    CHECK(metrics.accepted_total == 2);
    CHECK(metrics.rejected_after_close == 1);
}

TEST_CASE("a producer blocks while the queue is full and continues once space appears",
          "[queue][blocking]") {
    BoundedQueue<int> queue(2);
    REQUIRE(queue.push(1) == PushStatus::kAccepted);
    REQUIRE(queue.push(2) == PushStatus::kAccepted);

    std::atomic<bool> push_finished{false};
    std::atomic<int> push_status{-1};

    std::thread producer([&] {
        const PushStatus status = queue.push(3);
        push_status.store(static_cast<int>(status));
        push_finished.store(true);
    });

    // The queue is full, so the producer cannot complete no matter how slowly it
    // is scheduled. A sanitizer-slowed machine cannot turn this into a pass.
    std::this_thread::sleep_for(100ms);
    CHECK_FALSE(push_finished.load());
    CHECK(queue.size() == 2);

    // Making room releases it.
    const auto first = queue.pop();
    producer.join();

    REQUIRE(first.has_value());
    CHECK(*first == 1);
    CHECK(push_finished.load());
    CHECK(push_status.load() == static_cast<int>(PushStatus::kAccepted));
    CHECK(queue.size() == 2);

    const auto second = queue.pop();
    const auto third = queue.pop();
    REQUIRE(second.has_value());
    REQUIRE(third.has_value());
    CHECK(*second == 2);
    CHECK(*third == 3);

    CHECK(queue.metrics().high_water_depth == 2);
}

TEST_CASE("a blocked consumer wakes when an item arrives", "[queue][blocking]") {
    BoundedQueue<std::string> queue(4);

    std::atomic<bool> pop_finished{false};
    std::string received;

    std::thread consumer([&] {
        auto value = queue.pop();
        if (value.has_value()) {
            received = *value;
        }
        pop_finished.store(true);
    });

    // Nothing to take yet, so the consumer stays parked.
    std::this_thread::sleep_for(100ms);
    CHECK_FALSE(pop_finished.load());

    REQUIRE(queue.push("frame") == PushStatus::kAccepted);
    consumer.join();

    CHECK(pop_finished.load());
    CHECK(received == "frame");
}

// The two wait sets cannot both be occupied on one queue at the same time: live
// consumers keep it from staying full, and a full queue means consumers are not
// waiting. They get one test each.

TEST_CASE("closing wakes every blocked consumer", "[queue][close][blocking]") {
    BoundedQueue<int> queue(4);

    constexpr std::size_t kConsumerCount = 4;
    std::atomic<std::size_t> woken{0};
    std::atomic<std::size_t> received_values{0};

    std::vector<std::thread> consumers;
    consumers.reserve(kConsumerCount);
    for (std::size_t i = 0; i < kConsumerCount; ++i) {
        consumers.emplace_back([&] {
            const auto value = queue.pop();
            if (value.has_value()) {
                ++received_values;
            }
            ++woken;
        });
    }

    // Nothing is ever pushed, so all four are parked in pop().
    std::this_thread::sleep_for(100ms);
    CHECK(woken.load() == 0);

    queue.close();
    for (auto& consumer : consumers) {
        consumer.join();
    }

    // Every consumer returned rather than hanging, and each saw end-of-stream.
    CHECK(woken.load() == kConsumerCount);
    CHECK(received_values.load() == 0);
}

TEST_CASE("closing releases a producer blocked on a full queue", "[queue][close][blocking]") {
    BoundedQueue<int> queue(2);
    REQUIRE(queue.push(1) == PushStatus::kAccepted);
    REQUIRE(queue.push(2) == PushStatus::kAccepted);

    std::atomic<bool> push_finished{false};
    std::atomic<int> push_status{-1};

    std::thread producer([&] {
        const PushStatus status = queue.push(3);
        push_status.store(static_cast<int>(status));
        push_finished.store(true);
    });

    std::this_thread::sleep_for(100ms);
    CHECK_FALSE(push_finished.load());

    queue.close();
    producer.join();

    CHECK(push_finished.load());
    CHECK(push_status.load() == static_cast<int>(PushStatus::kClosed));

    // The blocked item was discarded, but the two accepted before close survive.
    const auto first = queue.pop();
    const auto second = queue.pop();
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK(*first == 1);
    CHECK(*second == 2);
    CHECK_FALSE(queue.pop().has_value());

    CHECK(queue.metrics().accepted_total == 2);
    CHECK(queue.metrics().rejected_after_close == 1);
}

TEST_CASE("metrics track depth and high water mark", "[queue][metrics]") {
    BoundedQueue<int> queue(4);

    CHECK(queue.metrics().depth == 0);
    CHECK(queue.metrics().high_water_depth == 0);

    for (int value = 1; value <= 3; ++value) {
        REQUIRE(queue.push(value) == PushStatus::kAccepted);
    }

    CHECK(queue.metrics().depth == 3);
    CHECK(queue.metrics().high_water_depth == 3);

    CHECK(queue.pop().has_value());
    CHECK(queue.pop().has_value());

    // Depth falls back but the high water mark records that the burst happened.
    CHECK(queue.metrics().depth == 1);
    CHECK(queue.metrics().high_water_depth == 3);
    CHECK(queue.metrics().accepted_total == 3);
}

TEST_CASE("many producers and consumers move every item exactly once",
          "[queue][concurrency][stress]") {
    constexpr std::size_t kProducers = 4;
    constexpr std::size_t kConsumers = 4;
    constexpr int kPerProducer = 500;

    // A small capacity relative to the traffic guarantees the producers really
    // do block and the wait/notify paths are exercised, not skipped.
    BoundedQueue<int> queue(16);

    std::mutex results_mutex;
    std::vector<int> received;
    received.reserve(kProducers * static_cast<std::size_t>(kPerProducer));

    std::vector<std::thread> consumers;
    consumers.reserve(kConsumers);
    for (std::size_t i = 0; i < kConsumers; ++i) {
        consumers.emplace_back([&] {
            std::vector<int> local;
            while (true) {
                auto value = queue.pop();
                if (!value.has_value()) {
                    break;
                }
                local.push_back(*value);
            }
            const std::lock_guard<std::mutex> lock(results_mutex);
            received.insert(received.end(), local.begin(), local.end());
        });
    }

    std::vector<std::thread> producers;
    producers.reserve(kProducers);
    for (std::size_t p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, p] {
            const int base = static_cast<int>(p) * kPerProducer;
            for (int i = 0; i < kPerProducer; ++i) {
                if (queue.push(base + i) != PushStatus::kAccepted) {
                    return;
                }
            }
        });
    }

    for (auto& producer : producers) {
        producer.join();
    }
    queue.close();
    for (auto& consumer : consumers) {
        consumer.join();
    }

    // Nothing lost, nothing duplicated: the received set is exactly the set of
    // values that were pushed.
    std::sort(received.begin(), received.end());
    std::vector<int> expected(kProducers * static_cast<std::size_t>(kPerProducer));
    std::iota(expected.begin(), expected.end(), 0);

    CHECK(received.size() == expected.size());
    CHECK(received == expected);
    CHECK(queue.metrics().accepted_total == expected.size());
    CHECK(queue.metrics().high_water_depth <= queue.capacity());
}
