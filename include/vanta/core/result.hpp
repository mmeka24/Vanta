#pragma once

#include <cstddef>
#include <utility>
#include <variant>

namespace vanta {

/// Minimal "value or structured error" type.
///
/// The project targets C++20, where std::expected is not yet available. This
/// exists so that malformed external input returns an explicit error instead of
/// throwing or silently producing a default value.
template <class T, class E>
class Result {
  public:
    [[nodiscard]] static Result ok(T value) {
        return Result(std::in_place_index<0>, std::move(value));
    }

    [[nodiscard]] static Result err(E error) {
        return Result(std::in_place_index<1>, std::move(error));
    }

    [[nodiscard]] bool hasValue() const noexcept {
        return storage_.index() == 0;
    }

    explicit operator bool() const noexcept {
        return hasValue();
    }

    /// Precondition: hasValue(). Violating it throws std::bad_variant_access
    /// rather than returning uninitialized memory.
    [[nodiscard]] const T& value() const {
        return std::get<0>(storage_);
    }

    /// Precondition: !hasValue().
    [[nodiscard]] const E& error() const {
        return std::get<1>(storage_);
    }

    [[nodiscard]] T valueOr(T fallback) const {
        return hasValue() ? std::get<0>(storage_) : std::move(fallback);
    }

  private:
    template <std::size_t I, class U>
    Result(std::in_place_index_t<I> tag, U&& initial) : storage_(tag, std::forward<U>(initial)) {}

    std::variant<T, E> storage_;
};

}  // namespace vanta
