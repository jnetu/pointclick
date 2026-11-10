#pragma once

#include <array>
#include <string_view>

// Clip names the player's animation set is authored with. Definition only: Room validates
// against them and Player picks from them, so neither depends on the other.

// Required in every room's set, in PlayerPose order.
inline constexpr std::array<std::string_view, 5> playerLocomotionAnimations{
    "idle", "left", "right", "up", "down"
};

// Optional standing clips per facing, in Facing order; "idle" stands in for a missing one.
inline constexpr std::array<std::string_view, 4> playerIdleAnimations{
    "idle_left", "idle_right", "idle_up", "idle_down"
};
