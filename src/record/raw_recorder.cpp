#include "vanta/record/raw_recorder.hpp"

#include <array>
#include <cctype>
#include <string>

#include "vanta/events/serialize.hpp"

namespace vanta {

namespace {

/// Substrings that never appear in an Alpaca quote or trade frame but do appear
/// in authentication traffic. Matched against a lowercased copy of the payload.
constexpr std::array<std::string_view, 5> kCredentialMarkers{"key_id", "secret", "authorization",
                                                             "apca-api", "\"action\":\"auth\""};

std::string toLowerCopy(std::string_view text) {
    std::string lowered;
    lowered.reserve(text.size());
    for (const char c : text) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return lowered;
}

/// Redaction marker. Deliberately carries no payload field at all, so it cannot
/// be mistaken for a frame that genuinely arrived with an empty payload.
std::string redactionLine(const RawFrame& frame) {
    std::string line = "{\"seq\":";
    line += std::to_string(frame.sequence.value());
    line += ",\"source\":\"";
    line += describe(frame.source);
    line += "\",\"arrival_ns\":";
    line += std::to_string(frame.arrival.nanos());
    line += ",\"redacted\":true}";
    return line;
}

}  // namespace

bool looksLikeCredentialPayload(std::string_view payload) noexcept {
    const std::string lowered = toLowerCopy(payload);
    for (const std::string_view marker : kCredentialMarkers) {
        if (lowered.find(marker) != std::string::npos) {
            return true;
        }
    }
    return false;
}

RawRecorder::RawRecorder(const std::filesystem::path& path, bool flush_each_record)
    : stream_(path, std::ios::out | std::ios::app | std::ios::binary),
      flush_each_record_(flush_each_record) {}

RawRecorder::~RawRecorder() {
    close();
}

bool RawRecorder::isOpen() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stream_.is_open();
}

bool RawRecorder::record(const RawFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!stream_.is_open()) {
        return false;
    }

    const bool redacted = looksLikeCredentialPayload(frame.payload);
    const std::string line = redacted ? redactionLine(frame) : toJson(frame);

    stream_ << line << '\n';
    if (flush_each_record_) {
        stream_.flush();
    }

    if (!stream_.good()) {
        return false;
    }

    if (redacted) {
        ++stats_.redacted;
    } else {
        ++stats_.written;
    }
    return true;
}

bool RawRecorder::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!stream_.is_open()) {
        return false;
    }
    stream_.flush();
    return stream_.good();
}

void RawRecorder::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!stream_.is_open()) {
        return;
    }
    stream_.flush();
    stream_.close();
}

RawRecorder::Stats RawRecorder::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

}  // namespace vanta
