#pragma once

#include "game/StringMap.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct SceneObject;

// A flag/counter always stays inside [-kMaxFlagValue, kMaxFlagValue]: setFlag and incrementFlag
// saturate there and a snapshot (so a save) outside it is refused. Well inside int, so no sum of
// a stored value with an `add_flag` amount can overflow.
inline constexpr int kMaxFlagValue = 1'000'000'000;

struct ObjectStateOverride {
    std::optional<bool> visible;
    std::optional<bool> solidOverride;
    // The clip `set_animation` left the object in (a door that stays open). Only the name: a
    // clip the room no longer has is ignored and the object shows its initial animation.
    std::optional<std::string> animation;
};

// Value copy of the whole SessionState, for saves. snapshot() sorts every list (items keep the
// inventory order), so the same progress always serializes to the same file.
struct SessionSnapshot {
    struct Flag {
        std::string id;
        int value = 0;
        bool operator==(const Flag&) const = default;
    };
    struct ObjectOverride {
        std::string roomId;
        std::string objectId;
        std::optional<bool> visible;
        std::optional<bool> solidOverride;
        std::optional<std::string> animation; // clip name; unknown to the room = harmless
        bool operator==(const ObjectOverride&) const = default;
    };
    struct DialogueChoice {
        std::string roomId;
        std::string nodeId;
        std::string choiceId;
        bool operator==(const DialogueChoice&) const = default;
    };

    std::vector<Flag> flags;               // by id
    std::vector<std::string> items;        // inventory order
    std::vector<ObjectOverride> objects;   // by room, then object; at least one value each
    std::vector<DialogueChoice> dialogueChoices; // by room, node, choice

    // Identifiers, duplicates, flag values (never 0, inside +-kMaxFlagValue) and empty
    // overrides. Empty means it can be restored.
    [[nodiscard]] std::string validationError() const;
    bool operator==(const SessionSnapshot&) const = default;
};

// Persistent gameplay progress across room loads and live DebugEditor edits.
// Authored definitions (Room, SceneObject) remain immutable during gameplay.
class SessionState {
public:
    void reset();

    [[nodiscard]] SessionSnapshot snapshot() const;
    // Replaces the whole progress. Validates first: on false nothing changed.
    bool restore(const SessionSnapshot& snapshot, std::string& error);

    // Boolean flags and integer counters (0 / absent means cleared). Values saturate at
    // +-kMaxFlagValue; incrementFlag returns the value that was stored.
    void setFlag(std::string_view id, int value = 1);
    void clearFlag(std::string_view id);
    [[nodiscard]] bool hasFlag(std::string_view id) const;
    [[nodiscard]] int flagValue(std::string_view id) const;
    int incrementFlag(std::string_view id, int delta = 1);

    // Ordered inventory items identified by stable IDs.
    bool addItem(std::string_view itemId);
    bool removeItem(std::string_view itemId);
    [[nodiscard]] bool hasItem(std::string_view itemId) const;
    [[nodiscard]] const std::vector<std::string>& items() const noexcept;

    // Per-room object overrides (visibility for pickups, solidity for doors/puzzles).
    void setObjectVisible(std::string_view roomId, std::string_view objectId, bool visible);
    [[nodiscard]] bool isObjectVisible(std::string_view roomId, std::string_view objectId) const;

    void setObjectSolid(std::string_view roomId, std::string_view objectId, bool solid);
    void clearObjectSolidOverride(std::string_view roomId, std::string_view objectId);
    [[nodiscard]] std::optional<bool> objectSolidOverride(std::string_view roomId,
                                                          std::string_view objectId) const;
    [[nodiscard]] bool isObjectSolid(std::string_view roomId, const SceneObject& object) const;

    // The clip an object keeps across room changes and saves (set_animation); empty = none,
    // the object starts at its initialAnimation.
    void setObjectAnimation(std::string_view roomId, std::string_view objectId,
                            std::string_view animation);
    [[nodiscard]] std::string_view objectAnimation(std::string_view roomId,
                                                   std::string_view objectId) const;

    // Dialogue progress: which `once` choices were already taken, per room + node + choice.
    void markDialogueChoice(std::string_view roomId, std::string_view nodeId,
                            std::string_view choiceId);
    [[nodiscard]] bool dialogueChoiceTaken(std::string_view roomId, std::string_view nodeId,
                                           std::string_view choiceId) const;

private:
    [[nodiscard]] const ObjectStateOverride* findOverride(std::string_view roomId,
                                                          std::string_view objectId) const;

    StringMap<int> flags_;
    std::vector<std::string> items_;
    StringMap<StringMap<ObjectStateOverride>> roomOverrides_;
    // "room/node/choice"; identifiers never contain '/'.
    std::unordered_set<std::string, TransparentStringHash, std::equal_to<>> dialogueChoices_;
};
