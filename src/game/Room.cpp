#include "game/Room.hpp"

#include "game/World.hpp"

#include <algorithm>

float Room::depthAt(const float feetY) const {
    const float range = std::max(nearDepthY - farDepthY, 1.0F);
    return std::clamp((feetY - farDepthY) / range, 0.0F, 1.0F);
}

float Room::scaleAt(const float feetY) const {
    return std::lerp(farScale, nearScale, depthAt(feetY));
}

float Room::speedAt(const float feetY) const {
    return std::lerp(playerFarSpeed, playerNearSpeed, depthAt(feetY));
}

SDL_FRect Room::playerBounds(const SDL_FPoint feet) const {
    const float scale = scaleAt(feet.y);
    const float width = playerBaseSize.x * scale;
    const float height = playerBaseSize.y * scale;
    return {feet.x - width * 0.5F, feet.y - height, width, height};
}

bool Room::containsFloor(const SDL_FPoint point) const {
    return point.x >= 0.0F && point.x < World::width
           && point.y >= wallBottomY && point.y < World::height;
}

SDL_FPoint Room::clampDestination(const SDL_FPoint point) const {
    const float y = std::clamp(point.y, walkableTopY, static_cast<float>(World::height));
    const float halfWidth = playerBaseSize.x * scaleAt(y) * 0.5F;
    return {std::clamp(point.x, halfWidth, World::width - halfWidth), y};
}

Room Room::firstRoom() {
    Room room;
    room.scenery = {
            {{380.0F, 345.0F, 100.0F, 135.0F}, {89, 127, 157, 255}},
            {{810.0F, 465.0F, 110.0F, 125.0F}, {220, 74, 68, 255},
             SceneObjectType::solid, {14.0F, 92.0F, 82.0F, 33.0F}},
    };
    return room;
}
