#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

// std::string-keyed map queryable by std::string_view without temporaries. Use it for ID
// lookups that run per frame or per tick.
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
