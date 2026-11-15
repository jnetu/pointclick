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
    // Unset (0, 0) is the room's size; validation keeps any other size positive on both sides.
    const bool roomSized = layer.size.x == 0.0F && layer.size.y == 0.0F;
    return {layer.position.x - cameraOffset.x * layer.scrollFactor.x,
            layer.position.y - cameraOffset.y * layer.scrollFactor.y,
            roomSized ? room.width : layer.size.x, roomSized ? room.height : layer.size.y};
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

SDL_FPoint layerCoverSize(const RoomLayer& layer, const Room& room) {
    const auto side = [](const float roomSide, const int viewSide, const float factor) {
        const auto view = static_cast<float>(viewSide);
        return view + std::max(roomSide - view, 0.0F) * factor;
    };
    return {side(room.width, World::width, layer.scrollFactor.x),
            side(room.height, World::height, layer.scrollFactor.y)};
}

bool hasLayers(const Room& room, const LayerPlane plane) {
    return std::ranges::any_of(room.layers,
                               [plane](const RoomLayer& layer) { return layer.plane == plane; });
}
