#pragma once

#include <string>
#include <variant>

// Outbound notifications from the domain, in the order they happened. Game appends them;
// core/Application drains the queue once per tick and routes each event (room loading and the
// autosave's stable points today; audio, transitions and analytics later) without Game knowing
// those systems.
// A new kind of notification = one struct here + one emit point in Game.

// A load committed, the player at the room's entry point (spawnId, or playerStart when
// empty); live DebugEditor edits do not emit it.
struct RoomEntered {
    std::string roomId;
    std::string spawnId;
};

// A finished script asked for another room (change_room <room> [spawn]). Application validates
// and loads <roomId>.room with the player at spawnId (empty = playerStart).
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
