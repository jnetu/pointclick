#pragma once

#include <string>
#include <variant>

// Outbound notifications from the domain, in the order they happened. Game appends them;
// core/Application drains the queue once per tick and routes each event (room loading today;
// audio, transitions, autosave and analytics later) without Game knowing those systems.
// A new kind of notification = one struct here + one emit point in Game.

// A load (spawn at playerStart) committed; live DebugEditor edits do not emit it.
struct RoomEntered {
    std::string roomId;
};

// A finished script asked for another room. Application validates and loads <roomId>.room.
struct RoomChangeRequested {
    std::string roomId;
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
