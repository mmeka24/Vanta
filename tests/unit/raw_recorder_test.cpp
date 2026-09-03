#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "vanta/events/raw_frame.hpp"
#include "vanta/record/raw_recorder.hpp"

using vanta::FrameSource;
using vanta::RawFrame;
using vanta::RawRecorder;
using vanta::SequenceNumber;
using vanta::Timestamp;

namespace {

/// Removes the file if it exists so each test starts from a known state. The
/// recorder appends, so a leftover file from an earlier run would corrupt the
/// line counts below.
std::filesystem::path freshPath(std::string_view name) {
    std::filesystem::path path = std::filesystem::temp_directory_path();
    path /= std::string("vanta_raw_recorder_") + std::string(name) + ".ndjson";
    std::filesystem::remove(path);
    return path;
}

/// Reads the log the way a replay source will: one line at a time.
std::vector<std::string> readLines(const std::filesystem::path& path) {
    std::vector<std::string> lines;
    std::ifstream input(path, std::ios::in | std::ios::binary);
    std::string line;
    while (std::getline(input, line)) {
        lines.push_back(line);
    }
    return lines;
}

/// Test-local JSON string unescaper.
///
/// Written independently of escapeJson on purpose: round-tripping a payload
/// through the implementation's own inverse would only prove the function is
/// self-consistent, not that it produced valid JSON.
std::string unescapeJsonString(std::string_view escaped) {
    std::string out;
    out.reserve(escaped.size());

    for (std::size_t i = 0; i < escaped.size(); ++i) {
        if (escaped[i] != '\\') {
            out.push_back(escaped[i]);
            continue;
        }

        ++i;
        REQUIRE(i < escaped.size());
        switch (escaped[i]) {
            case '"':
                out.push_back('"');
                break;
            case '\\':
                out.push_back('\\');
                break;
            case '/':
                out.push_back('/');
                break;
            case 'b':
                out.push_back('\b');
                break;
            case 'f':
                out.push_back('\f');
                break;
            case 'n':
                out.push_back('\n');
                break;
            case 'r':
                out.push_back('\r');
                break;
            case 't':
                out.push_back('\t');
                break;
            case 'u': {
                REQUIRE(i + 4 < escaped.size());
                unsigned int code = 0;
                for (std::size_t digit = 1; digit <= 4; ++digit) {
                    const char c = escaped[i + digit];
                    unsigned int nibble = 0;
                    if (c >= '0' && c <= '9') {
                        nibble = static_cast<unsigned int>(c - '0');
                    } else if (c >= 'a' && c <= 'f') {
                        nibble = static_cast<unsigned int>(c - 'a') + 10U;
                    } else if (c >= 'A' && c <= 'F') {
                        nibble = static_cast<unsigned int>(c - 'A') + 10U;
                    } else {
                        FAIL("invalid \\u escape in recorded line");
                    }
                    code = code * 16U + nibble;
                }
                REQUIRE(code < 0x80U);
                out.push_back(static_cast<char>(code));
                i += 4;
                break;
            }
            default:
                FAIL("unknown escape sequence in recorded line");
        }
    }

    return out;
}

/// Pulls one string field out of a recorded line, honouring escaped quotes.
std::string extractStringField(std::string_view line, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\":\"";
    const std::size_t start = line.find(needle);
    REQUIRE(start != std::string_view::npos);

    const std::size_t valueStart = start + needle.size();
    std::size_t cursor = valueStart;
    while (cursor < line.size()) {
        if (line[cursor] == '\\') {
            cursor += 2;
            continue;
        }
        if (line[cursor] == '"') {
            break;
        }
        ++cursor;
    }
    REQUIRE(cursor <= line.size());

    return unescapeJsonString(line.substr(valueStart, cursor - valueStart));
}

RawFrame makeFrame(std::uint64_t sequence, std::string payload) {
    RawFrame frame;
    frame.sequence = SequenceNumber{sequence};
    frame.source = FrameSource::kMarketData;
    frame.arrival =
        Timestamp::fromNanos(1'700'000'000'000'000'000 + static_cast<std::int64_t>(sequence));
    frame.payload = std::move(payload);
    return frame;
}

}  // namespace

TEST_CASE("each frame becomes one parseable line", "[recorder][ndjson]") {
    const std::filesystem::path path = freshPath("one_line_per_frame");

    {
        RawRecorder recorder(path);
        REQUIRE(recorder.isOpen());
        for (std::uint64_t i = 1; i <= 3; ++i) {
            REQUIRE(recorder.record(makeFrame(i, R"([{"T":"q","S":"AAPL"}])")));
        }
    }

    const std::vector<std::string> lines = readLines(path);
    REQUIRE(lines.size() == 3);
    for (const std::string& line : lines) {
        CHECK(line.front() == '{');
        CHECK(line.back() == '}');
        // One object per line: no embedded newline split a record in half.
        CHECK(line.find('\n') == std::string::npos);
    }

    std::filesystem::remove(path);
}

TEST_CASE("payloads survive byte for byte, including escaped json", "[recorder][payload]") {
    const std::filesystem::path path = freshPath("payload_roundtrip");

    const std::vector<std::string> payloads{
        R"([{"T":"q","S":"AAPL","bp":190.25,"ap":190.27}])",
        R"(a "quoted" value)",
        R"(back\slash and \"escaped quote\")",
        "line\nbreak\tand\ttabs",
        std::string("control\x01\x1f chars", 15),
        "utf8 caf\xc3\xa9 \xe2\x9c\x93",
        "",
    };

    {
        RawRecorder recorder(path);
        REQUIRE(recorder.isOpen());
        for (std::size_t i = 0; i < payloads.size(); ++i) {
            REQUIRE(recorder.record(makeFrame(i + 1, payloads[i])));
        }
    }

    const std::vector<std::string> lines = readLines(path);
    REQUIRE(lines.size() == payloads.size());

    for (std::size_t i = 0; i < payloads.size(); ++i) {
        const std::string recovered = extractStringField(lines[i], "payload");
        CHECK(recovered == payloads[i]);
        CHECK(recovered.size() == payloads[i].size());
    }

    std::filesystem::remove(path);
}

TEST_CASE("multiple frames preserve sequence order", "[recorder][ordering]") {
    const std::filesystem::path path = freshPath("sequence_order");

    constexpr std::uint64_t kFrameCount = 50;
    {
        RawRecorder recorder(path);
        REQUIRE(recorder.isOpen());
        for (std::uint64_t i = 1; i <= kFrameCount; ++i) {
            REQUIRE(recorder.record(makeFrame(i, "payload-" + std::to_string(i))));
        }
    }

    const std::vector<std::string> lines = readLines(path);
    REQUIRE(lines.size() == kFrameCount);

    for (std::uint64_t i = 1; i <= kFrameCount; ++i) {
        const std::string expectedSequence = "{\"seq\":" + std::to_string(i) + ",";
        CHECK(lines[i - 1].rfind(expectedSequence, 0) == 0);
        CHECK(extractStringField(lines[i - 1], "payload") == "payload-" + std::to_string(i));
    }

    std::filesystem::remove(path);
}

TEST_CASE("closing and flushing keeps every accepted frame", "[recorder][shutdown]") {
    const std::filesystem::path path = freshPath("close_loses_nothing");

    constexpr std::uint64_t kFrameCount = 200;

    // flush_each_record disabled so the data really is sitting in the stream
    // buffer when close() runs. If close did not flush, lines would be missing.
    RawRecorder recorder(path, /*flush_each_record=*/false);
    REQUIRE(recorder.isOpen());
    for (std::uint64_t i = 1; i <= kFrameCount; ++i) {
        REQUIRE(recorder.record(makeFrame(i, "payload-" + std::to_string(i))));
    }

    CHECK(recorder.stats().written == kFrameCount);
    recorder.close();
    CHECK_FALSE(recorder.isOpen());

    const std::vector<std::string> lines = readLines(path);
    CHECK(lines.size() == kFrameCount);
    CHECK(extractStringField(lines.back(), "payload") == "payload-200");

    // close is idempotent and a closed recorder accepts nothing further.
    recorder.close();
    CHECK_FALSE(recorder.record(makeFrame(kFrameCount + 1, "after-close")));
    CHECK(readLines(path).size() == kFrameCount);

    std::filesystem::remove(path);
}

TEST_CASE("the log is append-only across recorder lifetimes", "[recorder][append]") {
    const std::filesystem::path path = freshPath("append_only");

    {
        RawRecorder first(path);
        REQUIRE(first.record(makeFrame(1, "first-session")));
        REQUIRE(first.record(makeFrame(2, "first-session")));
    }
    {
        RawRecorder second(path);
        REQUIRE(second.record(makeFrame(3, "second-session")));
    }

    const std::vector<std::string> lines = readLines(path);
    REQUIRE(lines.size() == 3);
    CHECK(extractStringField(lines[0], "payload") == "first-session");
    CHECK(extractStringField(lines[2], "payload") == "second-session");

    std::filesystem::remove(path);
}

TEST_CASE("credential payloads are detected", "[recorder][credentials]") {
    CHECK(vanta::looksLikeCredentialPayload(R"({"action":"auth","key":"AK123","secret":"abc"})"));
    CHECK(vanta::looksLikeCredentialPayload(R"({"key_id":"AK123"})"));
    CHECK(vanta::looksLikeCredentialPayload(R"({"APCA-API-KEY-ID":"AK123"})"));
    CHECK(vanta::looksLikeCredentialPayload(R"({"Authorization":"Bearer x"})"));

    // Ordinary market data must not trip the detector, or real frames would be
    // dropped from the recording.
    CHECK_FALSE(vanta::looksLikeCredentialPayload(
        R"([{"T":"q","S":"AAPL","bp":190.25,"ap":190.27,"bs":3,"as":1}])"));
    CHECK_FALSE(vanta::looksLikeCredentialPayload(R"([{"T":"t","S":"AAPL","p":190.26,"s":50}])"));
    CHECK_FALSE(vanta::looksLikeCredentialPayload(R"([{"T":"subscription","quotes":["AAPL"]}])"));
}

TEST_CASE("credential payloads are never written to the log", "[recorder][credentials]") {
    const std::filesystem::path path = freshPath("credentials_redacted");

    const std::string secretPayload = R"({"action":"auth","key":"AKREALKEY","secret":"s3cr3t"})";

    {
        RawRecorder recorder(path);
        REQUIRE(recorder.isOpen());
        REQUIRE(recorder.record(makeFrame(1, R"([{"T":"q","S":"AAPL"}])")));
        REQUIRE(recorder.record(makeFrame(2, secretPayload)));
        REQUIRE(recorder.record(makeFrame(3, R"([{"T":"t","S":"AAPL"}])")));

        CHECK(recorder.stats().written == 2);
        CHECK(recorder.stats().redacted == 1);
    }

    const std::vector<std::string> lines = readLines(path);
    REQUIRE(lines.size() == 3);

    // The frame is still accounted for, so the sequence has no unexplained gap.
    CHECK(lines[1] == R"({"seq":2,"source":"market_data","arrival_ns":1700000000000000002,)"
                      R"("redacted":true})");

    // No trace of the credential anywhere in the file.
    for (const std::string& line : lines) {
        CHECK(line.find("AKREALKEY") == std::string::npos);
        CHECK(line.find("s3cr3t") == std::string::npos);
        CHECK(line.find("secret") == std::string::npos);
    }

    std::filesystem::remove(path);
}

TEST_CASE("a recorder that cannot open its file fails without throwing", "[recorder][errors]") {
    std::filesystem::path path = std::filesystem::temp_directory_path();
    path /= "vanta_missing_directory_xyz";
    path /= "raw.ndjson";
    std::filesystem::remove_all(path.parent_path());

    RawRecorder recorder(path);
    CHECK_FALSE(recorder.isOpen());
    CHECK_FALSE(recorder.record(makeFrame(1, "payload")));
    CHECK_FALSE(recorder.flush());
    CHECK(recorder.stats().written == 0);
}
