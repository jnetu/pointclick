#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

// Where an actor looks while standing still. Authored on spawns ("left", "right", "up",
// "down"), kept by Player from its last step and saved with the feet.
enum class Facing { left, right, up, down };

inline constexpr std::array<std::string_view, 4> facingNames{"left", "right", "up", "down"};

[[nodiscard]] constexpr std::string_view facingName(const Facing facing) {
    return facingNames[static_cast<std::size_t>(facing)];
}

[[nodiscard]] constexpr std::optional<Facing> parseFacing(const std::string_view text) {
    for (std::size_t i = 0; i < facingNames.size(); ++i) {
        if (facingNames[i] == text) return static_cast<Facing>(i);
    }
    return std::nullopt;
}
