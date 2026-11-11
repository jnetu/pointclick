#include "graphics/DialogueMenuRenderer.hpp"

#include "game/Game.hpp"
#include "graphics/TextRenderer.hpp"
#include "ui/DialogueMenu.hpp"

#include <SDL3/SDL.h>

namespace {
constexpr SDL_Color kTextColor{205, 210, 220, 255};
constexpr SDL_Color kHighlightedTextColor{255, 240, 168, 255};
} // namespace

void DialogueMenuRenderer::draw(SDL_Renderer* renderer, TextRenderer& text, const Game& game,
                                const DialogueMenu& menu) {
    if (!game.awaitingDialogueChoice()) return;
    const std::size_t count = menu.rowCount();
    if (count == 0) return;

    SDL_BlendMode previousBlend = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &previousBlend);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    const SDL_FRect panel = menu.panelRect();
    SDL_SetRenderDrawColor(renderer, 12, 18, 28, 225);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 242, 217, 139, 255);
    SDL_RenderRect(renderer, &panel);

    const std::size_t highlighted = menu.highlighted();
    for (std::size_t i = 0; i < count; ++i) {
        const DialogueMenu::Row& row = menu.row(i);
        if (i == highlighted) {
            SDL_SetRenderDrawColor(renderer, 70, 82, 110, 235);
            SDL_RenderFillRect(renderer, &row.rect);
        }
        const SDL_Color color = i == highlighted ? kHighlightedTextColor : kTextColor;
        for (std::size_t line = 0; line < row.lines.size(); ++line) {
            text.draw(FontRole::choice, row.lines[line].in(row.label), menu.lineOrigin(i, line),
                      color);
        }
    }
    SDL_SetRenderDrawBlendMode(renderer, previousBlend);
}
