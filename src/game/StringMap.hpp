#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

// Hash map keyed by std::string that can be queried with std::string_view without building a
// temporary string (transparent hash + std::equal_to<>). Use it for every ID-keyed lookup
// that may run per frame or per tick.
struct TransparentStringHash {
    using is_transparent = void;

    [[nodiscard]] std::size_t operator()(const std::string_view value) const noexcept {
        return std::hash<std::string_view>{}(value);
    }
    [[nodiscard]] std::size_t operator()(const std::string& value) const noexcept {
        return std::hash<std::string_view>{}(value);
    }
    [[nodiscard]] std::size_t operator()(const char* value) const noexcept {
        return std::hash<std::string_view>{}(value);
    }
};

template <typename Value>
using StringMap = std::unordered_map<std::string, Value, TransparentStringHash, std::equal_to<>>;
