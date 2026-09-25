#include "graphics/RoomBackdropRenderer.hpp"

#include "game/Room.hpp"
#include "game/World.hpp"

#include <SDL3/SDL.h>
#include <algorithm>

void RoomBackdropRenderer::draw(SDL_Renderer* renderer, const Room& room) {
    const SDL_FRect wall{0.0F, 0.0F, static_cast<float>(World::width), room.wallBottomY};
    const SDL_FRect floor{0.0F, room.wallBottomY, static_cast<float>(World::width),
                          World::height - room.wallBottomY};
    SDL_SetRenderDrawColor(renderer, room.wallColor.r, room.wallColor.g,
                           room.wallColor.b, room.wallColor.a);
    SDL_RenderFillRect(renderer, &wall);
    SDL_SetRenderDrawColor(renderer, room.floorColor.r, room.floorColor.g,
                           room.floorColor.b, room.floorColor.a);
    SDL_RenderFillRect(renderer, &floor);

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
}
