#include "graphics/DebugOverlay.hpp"
#include "game/RoomParameters.hpp"
#include "debug/DebugEditor.hpp"
#include "game/Game.hpp"
#include <SDL3/SDL.h>
#include <algorithm>

void DebugOverlay::draw(SDL_Renderer* renderer, const Game& game, const DebugEditor& editor) {
    if (!editor.active()) return;
    SDL_SetRenderDrawColor(renderer, 255, 231, 116, 255);
    for (const auto& object : game.room().scenery) {
        if (object.type == SceneObjectType::solid) {
            const auto base = object.worldCollisionFootprint();
            SDL_RenderRect(renderer, &base);
        }
        SDL_RenderDebugText(renderer, object.bounds.x, object.bounds.y - 12, object.id.c_str());
    }
    if (!editor.panelVisible()) {
        const SDL_FRect badge{844, 12, 424, 28};
        SDL_SetRenderDrawColor(renderer, 12, 18, 28, 255);
        SDL_RenderFillRect(renderer, &badge);
        SDL_SetRenderDrawColor(renderer, 242, 217, 139, 255);
        SDL_RenderDebugText(renderer, badge.x + 8, badge.y + 10,
                            "DEBUG ativo | TAB: painel | ESC: sair");
        return;
    }
    SDL_SetRenderDrawColor(renderer, 12, 18, 28, 255);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 113, 171, 223, 255);
    SDL_RenderRect(renderer, &panel);
    const float x = panel.x + 12;
    const float y = panel.y + 12;
    SDL_SetRenderDrawColor(renderer, 242, 217, 139, 255);
    SDL_RenderDebugTextFormat(renderer, x, y, "DEBUG | sala: %s | ESC: sair", game.room().id.c_str());
    SDL_SetRenderDrawColor(renderer, 214, 223, 232, 255);
    SDL_RenderDebugText(renderer, x, y + 24, "CIMA/BAIXO: selecionar | ESQ/DIR: alterar");
    SDL_RenderDebugText(renderer, x, y + 40, "TAB: ocultar painel e testar a sala inteira.");
    const auto parameters = roomParameters();
    constexpr std::size_t pageSize = 16;
    const auto first = (editor.selected() / pageSize) * pageSize;
    for (std::size_t i = first; i < std::min(first + pageSize, parameters.size()); ++i) {
        const auto& parameter = parameters[i];
        const float rowY = y + 70 + static_cast<float>(i - first) * 20;
        if (editor.selected() == i) {
            SDL_SetRenderDrawColor(renderer, 38, 63, 90, 255);
            const SDL_FRect highlight{x - 4, rowY - 4, panel.w - 20, 18};
            SDL_RenderFillRect(renderer, &highlight);
        }
        SDL_SetRenderDrawColor(renderer, 226, 233, 241, 255);
        SDL_RenderDebugTextFormat(renderer, x, rowY, "%s %-25s %8.2f",
                                  editor.selected() == i ? ">" : " ",
                                  parameter.key.data(), parameter.read(game.room()));
    }
    const auto& selected = parameters[editor.selected()];
    SDL_SetRenderDrawColor(renderer, 165, 195, 218, 255);
    SDL_RenderDebugText(renderer, x, y + 402, selected.description.data());
    SDL_RenderDebugTextFormat(renderer, x, y + 420, "Intervalo: %.2f a %.2f | Passo: %.2f",
                              selected.minimum, selected.maximum, selected.step);
    SDL_RenderDebugText(renderer, x, y + 450, "set player.far_speed 140   | teleport 640 500");
    SDL_RenderDebugText(renderer, x, y + 468, "move red_box 850 460       | size red_box 110 125");
    SDL_RenderDebugText(renderer, x, y + 486, "base red_box 14 92 82 33   | solid red_box 1");
    SDL_RenderDebugText(renderer, x, y + 504, "save teste.room | load teste.room | undo | reset");
    SDL_SetRenderDrawColor(renderer, 242, 217, 139, 255);
    const auto visibleCommand = editor.command().size() > 60
                                ? editor.command().substr(editor.command().size() - 60) : editor.command();
    SDL_RenderDebugTextFormat(renderer, x, y + 542, "> %s_", visibleCommand.c_str());
    SDL_RenderDebugText(renderer, x, y + 560, "ENTER: aplicar | BACKSPACE: apagar");
    SDL_SetRenderDrawColor(renderer, 202, 215, 228, 255);
    for (std::size_t offset = 0; offset < editor.status().size() && offset < 180; offset += 60) {
        SDL_RenderDebugText(renderer, x, y + 596 + static_cast<float>(offset / 60) * 16,
                            editor.status().substr(offset, 60).c_str());
    }
    auto directory = editor.roomDirectory().string();
    if (directory.size() > 55) directory = "..." + directory.substr(directory.size() - 52);
    SDL_SetRenderDrawColor(renderer, 142, 161, 180, 255);
    SDL_RenderDebugTextFormat(renderer, x, y + 654, "Pasta: %s", directory.c_str());
}
