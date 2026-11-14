#pragma once

#include "game/RoomLayer.hpp"

#include <SDL3/SDL_rect.h>

struct SDL_Renderer;
struct Room;
class SpriteLibrary;

namespace LayerRenderer {
// Draws the room's layers on one plane, in file order, where layerRect places them. A layer
// without art draws the checkerboard like any missing sprite. Expects a validated room.
void draw(SDL_Renderer* renderer, SpriteLibrary& sprites, const Room& room, LayerPlane plane,
          SDL_FPoint cameraOffset);
}
