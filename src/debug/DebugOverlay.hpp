#pragma once

#include "graphics/RenderOverlay.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

class DebugEditor;

// Characters per row of the editor panel (debug font, 8 px each, panel 544 px wide).
inline constexpr std::size_t kPanelColumns = 60;

// The panel's text rows: breaks at '\n', then at the last space before kPanelColumns (or at
// kPanelColumns). The panel has no scrolling: past maxRows the last row ends in "...".
[[nodiscard]] std::vector<std::string> wrapPanelText(std::string_view text, std::size_t maxRows);

// Everything the tools draw: with the editor active, the panel, collision bases, object names
// and the DebugViews (route, diagnostics); with it closed, only the hint.
class DebugOverlay final : public RenderOverlay {
public:
    explicit DebugOverlay(const DebugEditor& editor) : editor_(editor) {}
    void draw(SDL_Renderer* renderer, const Game& game, float interpolation) const override;

private:
    const DebugEditor& editor_;
};
