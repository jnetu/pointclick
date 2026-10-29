#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct SceneObject;

struct ObjectStateOverride {
    std::optional<bool> visible;
    std::optional<bool> solidOverride;
};

// Persistent gameplay progress across room loads and live DebugEditor edits.
// Authored definitions (Room, SceneObject) remain immutable during gameplay.
class SessionState {
public:
    void reset();

    // Boolean flags and integer counters (0 / absent means cleared).
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

private:
    struct TransparentStringHash {
        using is_transparent = void;

        [[nodiscard]] std::size_t operator()(const std::string_view value) const noexcept {
            return std::hash<std::string_view>{}(value);
        }

        [[nodiscard]] std::size_t operator()(const std::string& value) const noexcept {
            return std::hash<std::string_view>{}(value);
        }
    };

    template <typename Value>
    using StringMap = std::unordered_map<std::string, Value, TransparentStringHash, std::equal_to<>>;

    [[nodiscard]] const ObjectStateOverride* findOverride(std::string_view roomId,
                                                          std::string_view objectId) const;

    StringMap<int> flags_;
    std::vector<std::string> items_;
    StringMap<StringMap<ObjectStateOverride>> roomOverrides_;
};
