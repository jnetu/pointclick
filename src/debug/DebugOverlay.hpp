#pragma once

#include "graphics/RenderOverlay.hpp"

class DebugEditor;

// Everything the tools draw: with the editor active, the panel, collision bases, object names
// and the DebugViews (route, diagnostics); with it closed, only the hint.
class DebugOverlay final : public RenderOverlay {
public:
    explicit DebugOverlay(const DebugEditor& editor) : editor_(editor) {}
    void draw(SDL_Renderer* renderer, const Game& game, float interpolation) const override;

private:
    const DebugEditor& editor_;
};
