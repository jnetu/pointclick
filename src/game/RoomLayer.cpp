#include "game/RoomLayer.hpp"

#include "game/Room.hpp"
#include "game/World.hpp"

#include <algorithm>

bool RoomLayer::operator==(const RoomLayer& other) const {
    return id == other.id && plane == other.plane && image == other.image
           && position.x == other.position.x && position.y == other.position.y
           && size.x == other.size.x && size.y == other.size.y
           && scrollFactor.x == other.scrollFactor.x && scrollFactor.y == other.scrollFactor.y;
}

SDL_FRect layerRect(const RoomLayer& layer, const Room& room, const SDL_FPoint cameraOffset) {
    const float width = layer.size.x > 0.0F ? layer.size.x : room.width;
    const float height = layer.size.y > 0.0F ? layer.size.y : room.height;
    return {layer.position.x - cameraOffset.x * layer.scrollFactor.x,
            layer.position.y - cameraOffset.y * layer.scrollFactor.y, width, height};
}

bool layerCoversViewport(const RoomLayer& layer, const Room& room) {
    const SDL_FPoint farthest{std::max(room.width - static_cast<float>(World::width), 0.0F),
                              std::max(room.height - static_cast<float>(World::height), 0.0F)};
    const auto covers = [&](const SDL_FPoint camera) {
        const SDL_FRect rect = layerRect(layer, room, camera);
        return rect.x <= 0.0F && rect.y <= 0.0F
               && rect.x + rect.w >= static_cast<float>(World::width)
               && rect.y + rect.h >= static_cast<float>(World::height);
    };
    return covers({0.0F, 0.0F}) && covers(farthest);
}
