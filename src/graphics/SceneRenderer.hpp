#pragma once

struct SDL_Renderer;
class Game;
class SpriteLibrary;

namespace SceneRenderer {
void draw(SDL_Renderer* renderer, SpriteLibrary& sprites,
          const Game& game, float interpolation);
}
