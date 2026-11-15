#pragma once

#include "game/SpriteClip.hpp"

#include <SDL3/SDL_rect.h>

#include <cstdint>
#include <string>

struct Room;

// Which band of the frame draws the layer: behind every object and the player, or over them.
enum class LayerPlane : std::uint8_t { back, front };

// A painted image of the room (`[layer <id>]`): visual only. It never takes part in picking,
// collision or depth; interaction belongs to the objects. Definition only, no progress.
struct RoomLayer {
    std::string id;
    LayerPlane plane = LayerPlane::back;
    // The PNG and its cut. display_* and offset_* do not apply to a layer (size says it all).
    SpriteClip image;
    // Top-left corner in room units, before the parallax shift.
    SDL_FPoint position{0.0F, 0.0F};
    // (0, 0) = the room's size, so a plain backdrop needs no key; otherwise both sides > 0.
    SDL_FPoint size{0.0F, 0.0F};
    // How much of the camera offset the layer follows: 1 = moves with the world, 0 = fixed on
    // the screen, between 0 and 1 = far parallax, above 1 = near foreground passing faster.
    SDL_FPoint scrollFactor{1.0F, 1.0F};

    [[nodiscard]] bool operator==(const RoomLayer& other) const;
};

// Pure layout shared by the renderer, the content check and the tests (no PNG is opened here).
// Where the layer lands in the viewport for a camera offset: position - camera * scroll_factor,
// with the unset (0, 0) size replaced by the room's size. Expects a validated layer.
[[nodiscard]] SDL_FRect layerRect(const RoomLayer& layer, const Room& room,
                                  SDL_FPoint cameraOffset);
// True when the layer fills the whole viewport for every camera offset the room allows
// ([0, room - World] on each axis). The rect is affine in the camera, so both ends suffice.
[[nodiscard]] bool layerCoversViewport(const RoomLayer& layer, const Room& room);
// The smallest size that covers the viewport for every camera offset with the layer at 0, 0:
// World + max(room - World, 0) * scroll_factor on each axis (the ArtCheck coverage warning).
[[nodiscard]] SDL_FPoint layerCoverSize(const RoomLayer& layer, const Room& room);
// True when the room declares at least one layer on that plane (a `back` one replaces the
// procedural backdrop, D8).
[[nodiscard]] bool hasLayers(const Room& room, LayerPlane plane);
