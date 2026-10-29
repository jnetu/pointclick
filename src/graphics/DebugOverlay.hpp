#pragma once

struct SDL_Renderer;
class Game;
class DebugEditor;

namespace DebugOverlay {
void draw(SDL_Renderer* renderer, const Game& game, const DebugEditor& editor,
          float interpolation = 1.0F);
}
