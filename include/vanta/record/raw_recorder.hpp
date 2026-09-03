#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

#include "vanta/events/raw_frame.hpp"

namespace vanta {

/// True when a payload looks like it carries authentication material.
///
/// This is a last-resort net, not the primary defence: the market-data client
/// should never hand an authentication frame to the recorder in the first place.
/// It exists because a credential written into an append-only log cannot be
/// taken back.
[[nodiscard]] bool looksLikeCredentialPayload(std::string_view payload) noexcept;

/// Append-only NDJSON writer for raw frames.
///
/// One frame becomes one line. The file is opened in append mode and never
/// rewritten, because raw.ndjson is the source of truth for replay: anything
/// that edits an earlier line destroys the ability to reproduce a session.
///
/// Thread-safe. Network threads may record concurrently.
class RawRecorder {
  public:
    struct Stats {
        std::uint64_t written{0};
        std::uint64_t redacted{0};
    };

    /// Opens path for append. Failure is reported through isOpen() rather than
    /// an exception, so a recorder that cannot open its file degrades to a
    /// no-op the caller can detect.
    ///
    /// flush_each_record trades throughput for durability. It defaults to true:
    /// a recorder that loses its last seconds of data on a crash defeats the
    /// purpose of recording.
    explicit RawRecorder(const std::filesystem::path& path, bool flush_each_record = true);

    ~RawRecorder();

    RawRecorder(const RawRecorder&) = delete;
    RawRecorder& operator=(const RawRecorder&) = delete;
    RawRecorder(RawRecorder&&) = delete;
    RawRecorder& operator=(RawRecorder&&) = delete;

    [[nodiscard]] bool isOpen() const;

    /// Appends one line. Returns false if the recorder is closed or the write
    /// failed. A payload that looks like credentials is replaced by a redaction
    /// marker carrying the same sequence number, so the gap is visible in the
    /// log instead of appearing as a silently missing frame.
    [[nodiscard]] bool record(const RawFrame& frame);

    [[nodiscard]] bool flush();

    /// Flushes and closes. Idempotent; the destructor calls it.
    void close();

    [[nodiscard]] Stats stats() const;

  private:
    mutable std::mutex mutex_;
    std::ofstream stream_;
    bool flush_each_record_;
    Stats stats_;
};

}  // namespace vanta
