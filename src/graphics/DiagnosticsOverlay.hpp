#pragma once

struct SDL_Renderer;
struct SDL_Window;
class Game;

namespace DiagnosticsOverlay {
void draw(SDL_Renderer* renderer, SDL_Window* window, const Game& game, float interpolation);
}
