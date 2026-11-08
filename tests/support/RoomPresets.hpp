#pragma once

#include "game/Interaction.hpp"
#include "game/Room.hpp"

// Test fixtures only (target pointclick_test_support): rooms, art and rules built in C++.
// Neither the game nor the room loader links them; every shipped room comes from assets/.
namespace RoomPresets {
// The player clips of the original placeholder art, for rooms built in tests.
[[nodiscard]] Room defaults();
[[nodiscard]] Room firstRoom();
[[nodiscard]] RoomInteractions firstRoomInteractions();
}
