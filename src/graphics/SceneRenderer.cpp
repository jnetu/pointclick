#include "graphics/SceneRenderer.hpp"
#include "graphics/EntityRenderer.hpp"
#include "graphics/RoomBackdropRenderer.hpp"
#include "graphics/SpriteLibrary.hpp"
#include "game/Scene.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <vector>

void SceneRenderer::draw(SDL_Renderer* renderer, SpriteLibrary& sprites,
                         const Scene& scene, const float interpolation) {
    const Room& room = scene.room();
    const SDL_FPoint cameraOffset = scene.camera().interpolatedPosition(interpolation);
    RoomBackdropRenderer::draw(renderer, room, cameraOffset);

    const SDL_FPoint feet = scene.player().interpolatedFeet(interpolation);
    const auto& scenery = room.scenery;
    struct DrawItem { float depth; int objectIndex; };
    thread_local std::vector<DrawItem> items;
    items.clear();
    items.reserve(scenery.size() + 1);
    for (std::size_t i = 0; i < scenery.size(); ++i) {
        if (!scene.isObjectVisible(scenery[i].id)) continue;
        items.push_back({scenery[i].depth(), static_cast<int>(i)});
    }
    items.push_back({feet.y, -1});
    std::stable_sort(items.begin(), items.end(), [](const DrawItem& a, const DrawItem& b) {
        return a.depth < b.depth;
    });
    for (const DrawItem item : items) {
        if (item.objectIndex < 0) {
            EntityRenderer::drawPlayer(sprites, room, scene.player(), feet, cameraOffset,
                                       interpolation);
        } else {
            const SceneObject& object = scenery[static_cast<std::size_t>(item.objectIndex)];
            EntityRenderer::drawObject(renderer, sprites, object,
                                       scene.objectAnimation(object.id), cameraOffset,
                                       interpolation);
        }
    }
}
