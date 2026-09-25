#pragma once

struct SDL_Renderer;
class Game;

namespace SceneRenderer {
void draw(SDL_Renderer* renderer, const Game& game, float interpolation);
}
