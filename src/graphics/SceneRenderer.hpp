#pragma once

struct SDL_Renderer;
class Scene;
class SpriteLibrary;

namespace SceneRenderer {
void draw(SDL_Renderer* renderer, SpriteLibrary& sprites,
          const Scene& scene, float interpolation);
}
