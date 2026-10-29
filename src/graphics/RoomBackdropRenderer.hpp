#pragma once

#include <SDL3/SDL_rect.h>

struct SDL_Renderer;
struct Room;

namespace RoomBackdropRenderer {
void draw(SDL_Renderer* renderer, const Room& room, SDL_FPoint cameraOffset = {});
}
