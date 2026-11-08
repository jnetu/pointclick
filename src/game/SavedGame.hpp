#pragma once

#include "game/Facing.hpp"
#include "game/SessionState.hpp"

#include <SDL3/SDL_rect.h>

#include <string>

// Only room, feet, facing and progress. Never the entry spawn (moving spawns must not break
// saves); definitions and simulation (camera, animations, route, scripts) are rebuilt on load.
struct SavedGame {
    std::string roomId;
    SDL_FPoint feet{};
    SessionSnapshot session;
    Facing facing = Facing::down;

    bool operator==(const SavedGame& other) const {
        return roomId == other.roomId && feet.x == other.feet.x && feet.y == other.feet.y
               && facing == other.facing && session == other.session;
    }
};
