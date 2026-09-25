#include "graphics/DiagnosticsOverlay.hpp"
#include "game/Game.hpp"
#include "game/World.hpp"
#include <SDL3/SDL.h>

namespace {
enum class Anchor { topLeft, bottomRight };

SDL_FPoint anchored(const Anchor anchor, const float width, const float height,
                    const float margin) {
    if (anchor == Anchor::topLeft) {
        return {margin, margin};
    }
    return {World::width - width - margin, World::height - height - margin};
}

}

void DiagnosticsOverlay::draw(SDL_Renderer* renderer, SDL_Window* window, const Game& game,
                              const float interpolation) {
    const Room& room = game.room();
    const auto feet = game.player().interpolatedFeet(interpolation);
    const auto destination = game.player().destination();
    int windowWidth = 0;
    int windowHeight = 0;
    int outputWidth = 0;
    int outputHeight = 0;
    SDL_FRect viewport{};
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    SDL_GetRenderOutputSize(renderer, &outputWidth, &outputHeight);
    SDL_GetRenderLogicalPresentationRect(renderer, &viewport);

    const SDL_FPoint panel = anchored(Anchor::topLeft, 444.0F, 208.0F, 12.0F);
    const SDL_FRect panelRect{panel.x, panel.y, 444.0F, 208.0F};
    SDL_SetRenderDrawColor(renderer, 9, 12, 19, 230);
    SDL_RenderFillRect(renderer, &panelRect);
    SDL_SetRenderDrawColor(renderer, 198, 215, 231, 255);
    SDL_RenderDebugText(renderer, panel.x + 10.0F, panel.y + 10.0F,
                        "POINT & CLICK  |  left click: move");
    SDL_RenderDebugTextFormat(renderer, panel.x + 10.0F, panel.y + 28.0F,
                              "player feet: (%.1f, %.1f)  %s", feet.x, feet.y,
                              game.player().moving() ? "moving" : "idle");
    SDL_RenderDebugTextFormat(renderer, panel.x + 10.0F, panel.y + 44.0F,
                              "destination: (%.1f, %.1f)  route: %zu",
                              destination.x, destination.y, game.player().remainingWaypoints());
    SDL_RenderDebugTextFormat(renderer, panel.x + 10.0F, panel.y + 60.0F,
                              "mouse world: (%.1f, %.1f)  %s",
                              game.pointer().x, game.pointer().y,
                              game.pointerInside() ? "inside" : "outside");
    SDL_RenderDebugTextFormat(renderer, panel.x + 10.0F, panel.y + 76.0F,
                              "window: %d x %d  output: %d x %d",
                              windowWidth, windowHeight, outputWidth, outputHeight);
    SDL_RenderDebugTextFormat(renderer, panel.x + 10.0F, panel.y + 92.0F,
                              "world: %d x %d  viewport: %.0f x %.0f",
                              World::width, World::height, viewport.w, viewport.h);
    SDL_RenderDebugTextFormat(renderer, panel.x + 10.0F, panel.y + 108.0F,
                              "viewport offset: (%.0f, %.0f)  scale: %.2f",
                              viewport.x, viewport.y, viewport.w / World::width);
    SDL_RenderDebugTextFormat(renderer, panel.x + 10.0F, panel.y + 124.0F,
                              "wall: y < %.0f  player depth scale: %.2f",
                              room.wallBottomY, room.scaleAt(feet.y));
    SDL_RenderDebugTextFormat(renderer, panel.x + 10.0F, panel.y + 140.0F,
                              "speed: %.1f px/s  far: %.0f  near: %.0f",
                              room.speedAt(feet.y), room.playerFarSpeed,
                              room.playerNearSpeed);
    const char* poseName = "idle";
    switch (game.player().pose()) {
    case PlayerPose::idle: break;
    case PlayerPose::left: poseName = "left"; break;
    case PlayerPose::right: poseName = "right"; break;
    case PlayerPose::up: poseName = "up"; break;
    case PlayerPose::down: poseName = "down"; break;
    }
    const SpriteClip& clip = room.playerSprites.forPose(game.player().pose());
    SDL_RenderDebugTextFormat(renderer, panel.x + 10.0F, panel.y + 156.0F,
                              "anim: %s  frame: %d/%d", poseName,
                              clip.frameAt(game.player().animationTime()) + 1, clip.frames);
    SDL_RenderDebugTextFormat(renderer, panel.x + 10.0F, panel.y + 172.0F,
                              "sprite: %s", clip.file.substr(0, 42).c_str());

    const SDL_FPoint hint = anchored(Anchor::bottomRight, 256.0F, 18.0F, 12.0F);
    SDL_SetRenderDrawColor(renderer, 160, 175, 190, 255);
    SDL_RenderDebugText(renderer, hint.x, hint.y, "Digite DEBUG para editar a sala");

}
