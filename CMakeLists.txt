cmake_minimum_required(VERSION 3.24)

project(
    vanta
    VERSION 0.1.0
    DESCRIPTION "Event-driven trading research engine"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(VANTA_BUILD_TESTS "Build the Vanta test suite" ON)
option(VANTA_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(VANTA_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(VANTA_ENABLE_THREAD_SANITIZER "Enable ThreadSanitizer" OFF)

# ThreadSanitizer shares shadow-memory machinery with AddressSanitizer and the
# two cannot be linked into the same binary, so they get separate presets.
if(VANTA_ENABLE_SANITIZERS AND VANTA_ENABLE_THREAD_SANITIZER)
    message(
        FATAL_ERROR
            "VANTA_ENABLE_SANITIZERS and VANTA_ENABLE_THREAD_SANITIZER are mutually exclusive. "
            "Use the debug preset for ASan/UBSan or the tsan preset for ThreadSanitizer.")
endif()

find_package(Threads REQUIRED)
find_package(ixwebsocket CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# Compiler warnings
#
# Vanta stores prices and money as fixed-point int64_t. -Wconversion,
# -Wsign-conversion, -Wdouble-promotion, and -Wfloat-equal exist so that an
# accidental narrowing or float promotion in arithmetic code fails the build
# instead of silently changing a price.
# ---------------------------------------------------------------------------
add_library(vanta_warnings INTERFACE)
target_compile_options(
    vanta_warnings
    INTERFACE $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
              -Wall
              -Wextra
              -Wpedantic
              -Wshadow
              -Wconversion
              -Wsign-conversion
              -Wdouble-promotion
              -Wfloat-equal
              -Wold-style-cast
              -Wcast-align
              -Wnon-virtual-dtor
              -Woverloaded-virtual
              -Wnull-dereference
              -Wformat=2
              -Wunused>
              $<$<CXX_COMPILER_ID:MSVC>:/W4
              /permissive->)

if(VANTA_WARNINGS_AS_ERRORS)
    target_compile_options(
        vanta_warnings INTERFACE $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Werror>
                                 $<$<CXX_COMPILER_ID:MSVC>:/WX>)
endif()

# ---------------------------------------------------------------------------
# Sanitizers (debug preset)
# ---------------------------------------------------------------------------
add_library(vanta_sanitizers INTERFACE)

if(VANTA_ENABLE_SANITIZERS OR VANTA_ENABLE_THREAD_SANITIZER)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        if(VANTA_ENABLE_THREAD_SANITIZER)
            set(VANTA_SANITIZER_FLAGS -fsanitize=thread -fno-omit-frame-pointer)
        else()
            set(VANTA_SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer
                                      -fno-sanitize-recover=undefined)
        endif()
        target_compile_options(vanta_sanitizers INTERFACE ${VANTA_SANITIZER_FLAGS} -g)
        target_link_options(vanta_sanitizers INTERFACE ${VANTA_SANITIZER_FLAGS})
    else()
        message(WARNING "A sanitizer was requested but none are configured for compiler "
                        "${CMAKE_CXX_COMPILER_ID}; continuing without them.")
    endif()
endif()

# ---------------------------------------------------------------------------
# Generated version header
# ---------------------------------------------------------------------------
set(VANTA_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
configure_file(include/vanta/version.hpp.in
               "${VANTA_GENERATED_INCLUDE_DIR}/vanta/version.hpp" @ONLY)

# ---------------------------------------------------------------------------
# vanta_core
#
# Domain types shared by the live and replay paths. Nothing in this target may
# perform network I/O or read the system clock.
# ---------------------------------------------------------------------------
add_library(
    vanta_core STATIC
    src/core/price.cpp
    src/events/event.cpp
    src/events/serialize.cpp
    src/record/raw_recorder.cpp)
target_include_directories(
    vanta_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${VANTA_GENERATED_INCLUDE_DIR}")
target_compile_features(vanta_core PUBLIC cxx_std_20)
target_link_libraries(vanta_core PUBLIC vanta_warnings vanta_sanitizers Threads::Threads)

# Live market-data boundary. Keeping network dependencies out of vanta_core
# preserves the future replay path's ability to link without socket behavior.
add_library(
    vanta_live STATIC
    src/live/alpaca_config.cpp
    src/live/alpaca_protocol.cpp
    src/live/alpaca_recorder.cpp
    src/live/ix_websocket.cpp)
target_include_directories(vanta_live PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(vanta_live PUBLIC cxx_std_20)
target_link_libraries(
    vanta_live
    PUBLIC vanta_core nlohmann_json::nlohmann_json spdlog::spdlog
    PRIVATE ixwebsocket::ixwebsocket vanta_warnings vanta_sanitizers)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(vanta src/main.cpp)
target_link_libraries(vanta PRIVATE vanta_core)

add_executable(vanta_recorder src/recorder_main.cpp)
target_link_libraries(vanta_recorder PRIVATE vanta_live vanta_warnings vanta_sanitizers)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(VANTA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
