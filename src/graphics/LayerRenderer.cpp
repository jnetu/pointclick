#include "graphics/LayerRenderer.hpp"

#include "game/Room.hpp"
#include "graphics/SpriteLibrary.hpp"

void LayerRenderer::draw(SDL_Renderer* /*renderer*/, SpriteLibrary& sprites, const Room& room,
                         const LayerPlane plane, const SDL_FPoint cameraOffset) {
    for (const RoomLayer& layer : room.layers) {
        if (layer.plane != plane) continue;
        // Layers are still images for now: animation time arrives with M2-14.
        (void)sprites.draw(layer.image, layerRect(layer, room, cameraOffset), 0.0F);
    }
}
