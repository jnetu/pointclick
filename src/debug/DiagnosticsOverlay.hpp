#pragma once

struct SDL_Renderer;
class Scene;

// Technical readout (feet, route, camera, viewport, clip). Tools only: DebugOverlay draws it
// while the editor is active, so a build without tools never shows it.
namespace DiagnosticsOverlay {
void draw(SDL_Renderer* renderer, const Scene& scene, float interpolation);
}
