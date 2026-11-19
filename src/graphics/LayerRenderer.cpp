#include "graphics/LayerRenderer.hpp"

#include "game/Room.hpp"
#include "game/World.hpp"
#include "graphics/SpriteLibrary.hpp"

void LayerRenderer::draw(SDL_Renderer* /*renderer*/, SpriteLibrary& sprites, const Room& room,
                         const LayerPlane plane, const SDL_FPoint cameraOffset) {
    for (const RoomLayer& layer : room.layers) {
        if (layer.plane != plane) continue;
        const SDL_FRect rect = layerRect(layer, room, cameraOffset);
        // Off the screen at this camera: no texture lookup and no checkerboard to fill.
        if (!World::intersectsViewport(rect)) continue;
        // Layers are still images for now: animation time arrives with M2-14.
        (void)sprites.draw(layer.image, rect, 0.0F);
    }
}
