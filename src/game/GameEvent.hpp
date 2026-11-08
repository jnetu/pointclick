#pragma once

#include <string>
#include <variant>

// Outbound domain notifications, in order. Application drains them once per tick, so Game never
// knows the consumers. New kind = one struct here + one emit point in Game.

// A load committed (player at spawnId, or playerStart); live editor edits do not emit it.
struct RoomEntered {
    std::string roomId;
    std::string spawnId;
};

// A finished script ran change_room <room> [spawn]; Application validates and loads it.
struct RoomChangeRequested {
    std::string roomId;
    std::string spawnId;
};

// A scripted line started (subtitles, voice-over, dialogue log).
struct LineSpoken {
    std::string text;
};

// inCutscene() flipped: input hints, cursor and HUD can follow without polling.
struct CutsceneStarted {};
struct CutsceneEnded {};

using GameEvent = std::variant<RoomEntered, RoomChangeRequested, LineSpoken,
                               CutsceneStarted, CutsceneEnded>;
