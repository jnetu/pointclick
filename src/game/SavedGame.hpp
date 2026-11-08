#pragma once

#include "game/SessionState.hpp"

#include <SDL3/SDL_rect.h>

#include <string>

// What a save keeps, and nothing else: the room being played (its file is <roomId>.room), where
// the feet stand in it and the whole progress. Definitions come back from the content files;
// camera, animations, the route and any running script are simulation rebuilt on load.
struct SavedGame {
    std::string roomId;
    SDL_FPoint feet{};
    SessionSnapshot session;

    bool operator==(const SavedGame& other) const {
        return roomId == other.roomId && feet.x == other.feet.x && feet.y == other.feet.y
               && session == other.session;
    }
};
