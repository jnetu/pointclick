#pragma once

#include <SDL3/SDL_rect.h>
struct SDL_Renderer;
class Game;
class DebugEditor;

namespace DebugOverlay {
inline constexpr SDL_FRect panel{724, 12, 544, 696};
void draw(SDL_Renderer* renderer, const Game& game, const DebugEditor& editor);
}
