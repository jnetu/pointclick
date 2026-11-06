#pragma once

struct SDL_Renderer;
class Game;

// A layer drawn after the scene and the gameplay UI, before the gamepad cursor. Optional tools
// (the editor overlay) plug in here so graphics/ never depends on them.
class RenderOverlay {
public:
    virtual ~RenderOverlay() = default;
    virtual void draw(SDL_Renderer* renderer, const Game& game, float interpolation) const = 0;
};
