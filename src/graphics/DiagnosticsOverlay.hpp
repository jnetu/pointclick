#pragma once

struct SDL_Renderer;
struct SDL_Window;
class Scene;

namespace DiagnosticsOverlay {
void draw(SDL_Renderer* renderer, SDL_Window* window, const Scene& scene, float interpolation);
}
