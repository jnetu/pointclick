#include "graphics/RoomBackdropRenderer.hpp"

#include "game/Room.hpp"

#include <SDL3/SDL.h>
#include <algorithm>

void RoomBackdropRenderer::draw(SDL_Renderer* renderer, const Room& room,
                                const SDL_FPoint cameraOffset) {
    const SDL_FRect wall{-cameraOffset.x, -cameraOffset.y, room.width, room.wallBottomY};
    const SDL_FRect floor{-cameraOffset.x, room.wallBottomY - cameraOffset.y,
                          room.width, room.height - room.wallBottomY};
    SDL_SetRenderDrawColor(renderer, room.wallColor.r, room.wallColor.g,
                           room.wallColor.b, room.wallColor.a);
    SDL_RenderFillRect(renderer, &wall);
    SDL_SetRenderDrawColor(renderer, room.floorColor.r, room.floorColor.g,
                           room.floorColor.b, room.floorColor.a);
    SDL_RenderFillRect(renderer, &floor);

    SDL_SetRenderDrawColor(renderer, room.floorGuideColor.r,
                           room.floorGuideColor.g, room.floorGuideColor.b,
                           room.floorGuideColor.a);
    const float rayStep = static_cast<float>(std::max(room.floorRaySpacing, 1));
    for (float x = 0.0F; x <= room.width; x += rayStep) {
        SDL_RenderLine(renderer,
                       room.vanishingPointX - cameraOffset.x,
                       room.wallBottomY - cameraOffset.y,
                       x - cameraOffset.x,
                       room.height - cameraOffset.y);
    }
    for (const float fraction : room.floorGuideFractions) {
        const float y = room.wallBottomY + fraction * fraction
                        * (room.height - room.wallBottomY);
        SDL_RenderLine(renderer, -cameraOffset.x, y - cameraOffset.y,
                       room.width - cameraOffset.x, y - cameraOffset.y);
    }
    SDL_SetRenderDrawColor(renderer, room.wallTrimColor.r, room.wallTrimColor.g,
                           room.wallTrimColor.b, room.wallTrimColor.a);
    SDL_RenderLine(renderer, -cameraOffset.x, room.wallBottomY - cameraOffset.y,
                   room.width - cameraOffset.x, room.wallBottomY - cameraOffset.y);
}
