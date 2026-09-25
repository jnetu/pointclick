#include "graphics/SceneRenderer.hpp"
#include "graphics/EntityRenderer.hpp"
#include "graphics/RoomBackdropRenderer.hpp"
#include "graphics/SpriteLibrary.hpp"
#include "game/Game.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <vector>

void SceneRenderer::draw(SDL_Renderer* renderer, SpriteLibrary& sprites,
                         const Game& game, const float interpolation) {
    const Room& room = game.room();
    RoomBackdropRenderer::draw(renderer, room);

    const SDL_FPoint feet = game.player().interpolatedFeet(interpolation);
    SDL_FPoint segmentStart = feet;
    SDL_SetRenderDrawColor(renderer, 237, 224, 157, 255);
    for (const SDL_FPoint waypoint : game.player().remainingPath()) {
        SDL_RenderLine(renderer, segmentStart.x, segmentStart.y,
                       waypoint.x, waypoint.y);
        segmentStart = waypoint;
    }
    const auto& scenery = room.scenery;
    struct DrawItem { float depth; int objectIndex; };
    std::vector<DrawItem> items;
    items.reserve(scenery.size() + 1);
    for (std::size_t i = 0; i < scenery.size(); ++i) {
        items.push_back({scenery[i].depth(), static_cast<int>(i)});
    }
    items.push_back({feet.y, -1});
    std::stable_sort(items.begin(), items.end(), [](const DrawItem& a, const DrawItem& b) {
        return a.depth < b.depth;
    });
    for (const DrawItem item : items) {
        if (item.objectIndex < 0) {
            EntityRenderer::drawPlayer(sprites, room, game.player(), feet);
        } else {
            const SceneObject& object = scenery[static_cast<std::size_t>(item.objectIndex)];
            EntityRenderer::drawObject(renderer, sprites, object, game.objectAnimation(object.id));
        }
    }

    const SDL_FPoint destination = game.player().destination();
    if (game.player().moving()) {
        SDL_SetRenderDrawColor(renderer, 246, 206, 124, 255);
        SDL_RenderLine(renderer, destination.x - 7.0F, destination.y,
                       destination.x + 7.0F, destination.y);
        SDL_RenderLine(renderer, destination.x, destination.y - 7.0F,
                       destination.x, destination.y + 7.0F);
    }

    const FloatingText& floating = game.floatingText();
    if (floating.isVisible()) {
        SDL_SetRenderDrawColor(renderer, 240, 220, 100, 255);
        SDL_SetRenderScale(renderer, 4.0F, 4.0F);
        SDL_RenderDebugText(renderer, floating.interpolatedX(interpolation) / 4.0F,
                            floating.y() / 4.0F, floating.text().c_str());
        SDL_SetRenderScale(renderer, 1.0F, 1.0F);
    }

}
