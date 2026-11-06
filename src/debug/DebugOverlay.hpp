#pragma once

#include "graphics/RenderOverlay.hpp"

class DebugEditor;

// Draws the editor panel, collision bases and hotspots while the editor is active.
class DebugOverlay final : public RenderOverlay {
public:
    explicit DebugOverlay(const DebugEditor& editor) : editor_(editor) {}
    void draw(SDL_Renderer* renderer, const Game& game, float interpolation) const override;

private:
    const DebugEditor& editor_;
};
