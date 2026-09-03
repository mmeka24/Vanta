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

if(VANTA_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        set(VANTA_SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer
                                  -fno-sanitize-recover=undefined)
        target_compile_options(vanta_sanitizers INTERFACE ${VANTA_SANITIZER_FLAGS} -g)
        target_link_options(vanta_sanitizers INTERFACE ${VANTA_SANITIZER_FLAGS})
    else()
        message(WARNING "VANTA_ENABLE_SANITIZERS is ON but the sanitizers are not configured "
                        "for compiler ${CMAKE_CXX_COMPILER_ID}; continuing without them.")
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
# Header-only for now. Later chunks add sources here; nothing in this target
# may perform network I/O.
# ---------------------------------------------------------------------------
add_library(vanta_core INTERFACE)
target_include_directories(
    vanta_core INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${VANTA_GENERATED_INCLUDE_DIR}")
target_compile_features(vanta_core INTERFACE cxx_std_20)
target_link_libraries(vanta_core INTERFACE vanta_warnings vanta_sanitizers)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(vanta src/main.cpp)
target_link_libraries(vanta PRIVATE vanta_core)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(VANTA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
