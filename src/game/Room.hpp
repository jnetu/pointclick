#pragma once

#include "game/SceneObject.hpp"
#include "game/Animation.hpp"
#include "game/Facing.hpp"
#include "game/World.hpp"

#include <SDL3/SDL_rect.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// A named way into the room (`[spawn <id>]`): change_room <room> <id> places the feet here,
// looking at `facing`. Without a spawn id the player enters at playerStart looking down.
struct RoomSpawn {
    std::string id;
    SDL_FPoint feet{};
    Facing facing = Facing::down;

    bool operator==(const RoomSpawn& other) const {
        return id == other.id && feet.x == other.feet.x && feet.y == other.feet.y
               && facing == other.facing;
    }
};

// What Room::validationIssue found and where: the loader maps it to the line that declared it.
struct RoomValidationIssue {
    enum class Subject : std::uint8_t { room, spawn, object };

    std::string message; // empty = valid
    Subject subject = Subject::room;
    // Position in Room::spawns / Room::scenery. Only says where the entry was declared (two
    // entries may share a bad ID); never use it to address an object.
    std::size_t index = 0;
    // The member at fault, by its .room key ("feet", "bounds", "collision",
    // "animation.initial"); empty when the whole block is to blame.
    std::string_view field{};
};

// Room definition only. Content supplies player art defaults when loading files.
// Pixel values always use logical world units; the camera viewport is World::width x World::height.
struct Room {
    static constexpr float maxDimension = 8192.0F;

    std::string id = "first";
    float width = static_cast<float>(World::width);
    float height = static_cast<float>(World::height);
    // Room geometry and the shared depth range for size and movement speed.
    float wallBottomY = 280.0F;
    float walkableTopY = 300.0F;
    float farDepthY = 300.0F;
    float nearDepthY = 720.0F;
    float farScale = 0.55F;
    float nearScale = 1.65F;

    // Placeholder room art and perspective guides.
    SDL_Color wallColor{171, 62, 67, 255};
    SDL_Color floorColor{42, 102, 175, 255};
    SDL_Color wallTrimColor{109, 39, 48, 255};
    SDL_Color floorGuideColor{68, 124, 190, 255};
    SDL_Color playerColor{245, 194, 102, 255}; // Legacy room-file fields, unused with PNGs.
    SDL_Color playerOutlineColor{255, 239, 205, 255};
    float vanishingPointX = 640.0F;
    int floorRaySpacing = 160;
    std::array<float, 4> floorGuideFractions{0.18F, 0.38F, 0.62F, 0.85F};
    // Player tuning for this room; the live position belongs to Player.
    SDL_FPoint playerStart{640.0F, 360.0F};
    std::vector<RoomSpawn> spawns;
    SDL_FPoint playerBaseSize{48.0F, 80.0F};
    float playerFarSpeed = 110.0F;
    float playerNearSpeed = 330.0F;
    AnimationSet playerAnimations;
    // Extra space around solid footprints before rasterizing the navigation grid.
    float collisionPaddingX = 40.0F;
    float collisionPaddingY = 8.0F;
    std::vector<SceneObject> scenery;

    [[nodiscard]] float scaleAt(float feetY) const;
    [[nodiscard]] float speedAt(float feetY) const;
    [[nodiscard]] SDL_FRect playerBounds(SDL_FPoint feet) const;
    [[nodiscard]] bool containsFloor(SDL_FPoint point) const;
    [[nodiscard]] SDL_FPoint clampDestination(SDL_FPoint point) const;
    // Single lookup by stable ID; never address objects by vector position.
    [[nodiscard]] const SceneObject* findObject(std::string_view objectId) const;
    [[nodiscard]] const RoomSpawn* findSpawn(std::string_view spawnId) const;
    // The entry for change_room: empty id = playerStart facing down; unknown id = nullopt.
    [[nodiscard]] std::optional<RoomSpawn> entryPoint(std::string_view spawnId) const;

    // Shared with loaders that refuse a bad ID on the header line itself.
    static constexpr std::string_view spawnIdError = "ID de spawn invalido ou repetido";
    static constexpr std::string_view objectIdError = "ID de objeto invalido ou repetido";

    // The single source of the room rules. Empty message means valid. Invalid designer data
    // must never enter the simulation.
    [[nodiscard]] RoomValidationIssue validationIssue() const;
    [[nodiscard]] std::string validationError() const;

private:
    [[nodiscard]] float depthAt(float feetY) const;
};
