#include "graphics/SceneRenderer.hpp"
#include "graphics/SpriteLibrary.hpp"
#include "game/Game.hpp"
#include "game/World.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <vector>

namespace {
void drawObject(SDL_Renderer* renderer, SpriteLibrary& sprites,
                const SceneObject& object, const float elapsedSeconds) {
    if (object.sprite) {
        const SDL_FPoint foot{object.bounds.x + object.bounds.w * 0.5F,
                              object.bounds.y + object.bounds.h};
        (void)sprites.draw(*object.sprite,
                     object.sprite->targetRect(foot, object.bounds.w, object.bounds.h),
                     elapsedSeconds);
        return;
    }
    SDL_SetRenderDrawColor(renderer, object.color.r, object.color.g, object.color.b, 255);
    SDL_RenderFillRect(renderer, &object.bounds);
    if (object.type == SceneObjectType::solid) {
        const SDL_FRect base = object.worldCollisionFootprint();
        SDL_SetRenderDrawColor(renderer, 130, 42, 39, 255);
        SDL_RenderFillRect(renderer, &base);
    }
    SDL_SetRenderDrawColor(renderer, 210, 218, 231, 255);
    SDL_RenderRect(renderer, &object.bounds);
}

void drawPlayer(SpriteLibrary& sprites, const Room& room,
                const Player& player, const SDL_FPoint feet) {
    const SpriteClip& clip = room.playerSprites.forPose(player.pose());
    (void)sprites.draw(clip, clip.targetRect(feet, room.playerBaseSize.x,
                                        room.playerBaseSize.y, room.scaleAt(feet.y)),
                 player.animationTime());
}
}

void SceneRenderer::draw(SDL_Renderer* renderer, SpriteLibrary& sprites,
                         const Game& game, const float interpolation) {
    const Room& room = game.room();
    const SDL_FRect wall{0.0F, 0.0F, static_cast<float>(World::width), room.wallBottomY};
    const SDL_FRect floor{0.0F, room.wallBottomY, static_cast<float>(World::width),
                          World::height - room.wallBottomY};
    SDL_SetRenderDrawColor(renderer, room.wallColor.r, room.wallColor.g,
                           room.wallColor.b, room.wallColor.a);
    SDL_RenderFillRect(renderer, &wall);
    SDL_SetRenderDrawColor(renderer, room.floorColor.r, room.floorColor.g,
                           room.floorColor.b, room.floorColor.a);
    SDL_RenderFillRect(renderer, &floor);

    // Guide lines converge on the back wall, suggesting a room's depth.
    SDL_SetRenderDrawColor(renderer, room.floorGuideColor.r,
                           room.floorGuideColor.g, room.floorGuideColor.b,
                           room.floorGuideColor.a);
    for (int x = 0; x <= World::width; x += std::max(room.floorRaySpacing, 1)) {
        SDL_RenderLine(renderer, room.vanishingPointX, room.wallBottomY,
                       static_cast<float>(x), static_cast<float>(World::height));
    }
    for (const float fraction : room.floorGuideFractions) {
        const float y = room.wallBottomY + fraction * fraction
                        * (World::height - room.wallBottomY);
        SDL_RenderLine(renderer, 0.0F, y, static_cast<float>(World::width), y);
    }
    SDL_SetRenderDrawColor(renderer, room.wallTrimColor.r, room.wallTrimColor.g,
                           room.wallTrimColor.b, room.wallTrimColor.a);
    SDL_RenderLine(renderer, 0.0F, room.wallBottomY,
                   static_cast<float>(World::width), room.wallBottomY);

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
            drawPlayer(sprites, room, game.player(), feet);
        } else {
            drawObject(renderer, sprites, scenery[static_cast<std::size_t>(item.objectIndex)],
                       game.sceneTime());
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
