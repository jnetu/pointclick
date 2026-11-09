#include "debug/DiagnosticsOverlay.hpp"
#include "game/Scene.hpp"
#include "game/World.hpp"
#include <SDL3/SDL.h>

void DiagnosticsOverlay::draw(SDL_Renderer* renderer, const Scene& scene,
                              const float interpolation) {
    const Room& room = scene.room();
    const auto feet = scene.player().interpolatedFeet(interpolation);
    const auto camera = scene.camera().interpolatedPosition(interpolation);
    const auto destination = scene.player().destination();
    int windowWidth = 0;
    int windowHeight = 0;
    int outputWidth = 0;
    int outputHeight = 0;
    SDL_FRect viewport{};
    // A renderer drawing to a surface (tests, captures) has no window.
    if (SDL_Window* window = SDL_GetRenderWindow(renderer)) {
        SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    }
    SDL_GetRenderOutputSize(renderer, &outputWidth, &outputHeight);
    SDL_GetRenderLogicalPresentationRect(renderer, &viewport);

    const SDL_FPoint panel{12.0F, 12.0F};
    const SDL_FRect panelRect{panel.x, panel.y, 444.0F, 224.0F};
    SDL_SetRenderDrawColor(renderer, 9, 12, 19, 230);
    SDL_RenderFillRect(renderer, &panelRect);
    SDL_SetRenderDrawColor(renderer, 198, 215, 231, 255);
    SDL_RenderDebugText(renderer, panel.x + 10.0F, panel.y + 10.0F,
                        "POINT & CLICK  |  left click: move");
    SDL_RenderDebugTextFormat(renderer, panel.x + 10.0F, panel.y + 28.0F,
                              "player feet: (%.1f, %.1f)  %s", feet.x, feet.y,
                              scene.player().moving() ? "moving" : "idle");
    SDL_RenderDebugTextFormat(renderer, panel.x + 10.0F, panel.y + 44.0F,
                              "destination: (%.1f, %.1f)  route: %zu",
                              destination.x, destination.y, scene.player().remainingWaypoints());
    SDL_RenderDebugTextFormat(renderer, panel.x + 10.0F, panel.y + 60.0F,
                              "mouse world: (%.1f, %.1f)  %s",
                              scene.pointer().x, scene.pointer().y,
                              scene.pointerInside() ? "inside" : "outside");
    SDL_RenderDebugTextFormat(renderer, panel.x + 10.0F, panel.y + 76.0F,
                              "room: %.0f x %.0f  camera: (%.1f, %.1f)",
                              room.width, room.height, camera.x, camera.y);
    SDL_RenderDebugTextFormat(renderer, panel.x + 10.0F, panel.y + 92.0F,
                              "window: %d x %d  output: %d x %d",
                              windowWidth, windowHeight, outputWidth, outputHeight);
    SDL_RenderDebugTextFormat(renderer, panel.x + 10.0F, panel.y + 108.0F,
                              "fov: %d x %d  viewport: %.0f x %.0f",
                              World::width, World::height, viewport.w, viewport.h);
    SDL_RenderDebugTextFormat(renderer, panel.x + 10.0F, panel.y + 124.0F,
                              "viewport offset: (%.0f, %.0f)  scale: %.2f",
                              viewport.x, viewport.y, viewport.w / World::width);
    SDL_RenderDebugTextFormat(renderer, panel.x + 10.0F, panel.y + 140.0F,
                              "wall: y < %.0f  player depth scale: %.2f",
                              room.wallBottomY, room.scaleAt(feet.y));
    SDL_RenderDebugTextFormat(renderer, panel.x + 10.0F, panel.y + 156.0F,
                              "speed: %.1f px/s  far: %.0f  near: %.0f",
                              room.speedAt(feet.y), room.playerFarSpeed,
                              room.playerNearSpeed);
    const char* poseName = "idle";
    switch (scene.player().pose()) {
    case PlayerPose::idle: break;
    case PlayerPose::left: poseName = "left"; break;
    case PlayerPose::right: poseName = "right"; break;
    case PlayerPose::up: poseName = "up"; break;
    case PlayerPose::down: poseName = "down"; break;
    }
    const SpriteClip* clip = findAnimation(room.playerAnimations,
                                          playerClipName(scene.player(), room.playerAnimations));
    SDL_RenderDebugTextFormat(renderer, panel.x + 10.0F, panel.y + 172.0F,
                              "anim: %s  frame: %d/%d", poseName,
                              clip ? clip->frameAt(scene.player().animationTime()) + 1 : 0,
                              clip ? clip->frames : 0);
    SDL_RenderDebugTextFormat(renderer, panel.x + 10.0F, panel.y + 188.0F,
                              "sprite: %.42s", clip ? clip->file.c_str() : "missing clip");
}
