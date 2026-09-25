#pragma once

#include <string_view>

// Shared format for room IDs, object IDs and animation names.
[[nodiscard]] constexpr bool validIdentifier(const std::string_view name) {
    return !name.empty() && name.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")
            == std::string_view::npos;
}
