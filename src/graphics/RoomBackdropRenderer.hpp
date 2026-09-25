#pragma once

struct SDL_Renderer;
struct Room;

namespace RoomBackdropRenderer {
void draw(SDL_Renderer* renderer, const Room& room);
}
