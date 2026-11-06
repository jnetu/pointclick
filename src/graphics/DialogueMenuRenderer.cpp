#include "graphics/DialogueMenuRenderer.hpp"

#include "game/Game.hpp"
#include "ui/DialogueMenu.hpp"

#include <SDL3/SDL.h>

namespace {
constexpr float kTextScale = 2.0F;
constexpr float kGlyph = 8.0F * kTextScale; // SDL debug font is 8x8
} // namespace

void DialogueMenuRenderer::draw(SDL_Renderer* renderer, const Game& game,
                                const DialogueMenu& menu) {
    if (!game.awaitingDialogueChoice()) return;
    const auto options = game.dialogueOptions();
    if (options.empty()) return;

    SDL_BlendMode previousBlend = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &previousBlend);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    const SDL_FRect panel = DialogueMenu::panelRect(options.size());
    SDL_SetRenderDrawColor(renderer, 12, 18, 28, 225);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 242, 217, 139, 255);
    SDL_RenderRect(renderer, &panel);

    const std::size_t highlighted = menu.highlighted();
    for (std::size_t i = 0; i < options.size(); ++i) {
        const SDL_FRect row = DialogueMenu::optionRect(i, options.size());
        if (i == highlighted) {
            SDL_SetRenderDrawColor(renderer, 70, 82, 110, 235);
            SDL_RenderFillRect(renderer, &row);
            SDL_SetRenderDrawColor(renderer, 255, 240, 168, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 205, 210, 220, 255);
        }
        SDL_SetRenderScale(renderer, kTextScale, kTextScale);
        SDL_RenderDebugTextFormat(renderer, (row.x + 12.0F) / kTextScale,
                                  (row.y + (row.h - kGlyph) * 0.5F) / kTextScale, "%zu. %s",
                                  i + 1, options[i]->text.c_str());
        SDL_SetRenderScale(renderer, 1.0F, 1.0F);
    }
    SDL_SetRenderDrawBlendMode(renderer, previousBlend);
}
