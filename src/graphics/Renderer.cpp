#include "graphics/Renderer.hpp"

#include "game/Game.hpp"
#include "game/World.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <vector>

namespace {
enum class Anchor { topLeft, bottomRight };

SDL_FPoint anchored(const Anchor anchor, const float width, const float height,
                    const float margin) {
    if (anchor == Anchor::topLeft) {
        return {margin, margin};
    }
    return {World::width - width - margin, World::height - height - margin};
}

void drawObject(SDL_Renderer* renderer, const SceneObject& object) {
    SDL_SetRenderDrawColor(renderer, object.color.r, object.color.g, object.color.b, 255);
    SDL_RenderFillRect(renderer, &object.bounds);
    if (object.type == SceneObjectType::solid) {
        const SDL_FRect base = object.worldCollisionFootprint();
        SDL_SetRenderDrawColor(renderer, 130, 42, 39, 255);
        SDL_RenderFillRect(renderer, &base);
    }
    SDL_SetRenderDrawColor(renderer, 210, 218, 231, 255);
    SDL_RenderRect(renderer, &object.bounds);
}

void drawPlayer(SDL_Renderer* renderer, const Room& room, const SDL_FPoint feet) {
    const SDL_FRect bounds = room.playerBounds(feet);
    SDL_SetRenderDrawColor(renderer, room.playerColor.r, room.playerColor.g,
                           room.playerColor.b, room.playerColor.a);
    SDL_RenderFillRect(renderer, &bounds);
    SDL_SetRenderDrawColor(renderer, room.playerOutlineColor.r,
                           room.playerOutlineColor.g, room.playerOutlineColor.b,
                           room.playerOutlineColor.a);
    SDL_RenderRect(renderer, &bounds);
}
}

bool Renderer::initialize(SDL_Window* window) {
    window_ = window;
    renderer_ = SDL_CreateRenderer(window, nullptr);
    if (renderer_ == nullptr) {
        SDL_Log("Could not create the renderer: %s", SDL_GetError());
        return false;
    }
    if (!SDL_SetRenderLogicalPresentation(renderer_, World::width, World::height,
                                          SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
        SDL_Log("Could not set logical presentation: %s", SDL_GetError());
        return false;
    }
    return true;
}

bool Renderer::windowToWorld(const float x, const float y, SDL_FPoint& result) const {
    return SDL_RenderCoordinatesFromWindow(renderer_, x, y, &result.x, &result.y);
}

void Renderer::render(const Game& game, const float interpolation) {
    SDL_SetRenderDrawColor(renderer_, 8, 10, 16, 255);
    SDL_RenderClear(renderer_);

    const Room& room = game.room();
    const SDL_FRect wall{0.0F, 0.0F, static_cast<float>(World::width), room.wallBottomY};
    const SDL_FRect floor{0.0F, room.wallBottomY, static_cast<float>(World::width),
                          World::height - room.wallBottomY};
    SDL_SetRenderDrawColor(renderer_, room.wallColor.r, room.wallColor.g,
                           room.wallColor.b, room.wallColor.a);
    SDL_RenderFillRect(renderer_, &wall);
    SDL_SetRenderDrawColor(renderer_, room.floorColor.r, room.floorColor.g,
                           room.floorColor.b, room.floorColor.a);
    SDL_RenderFillRect(renderer_, &floor);

    // Guide lines converge on the back wall, suggesting a room's depth.
    SDL_SetRenderDrawColor(renderer_, room.floorGuideColor.r,
                           room.floorGuideColor.g, room.floorGuideColor.b,
                           room.floorGuideColor.a);
    for (int x = 0; x <= World::width; x += std::max(room.floorRaySpacing, 1)) {
        SDL_RenderLine(renderer_, room.vanishingPointX, room.wallBottomY,
                       static_cast<float>(x), static_cast<float>(World::height));
    }
    for (const float fraction : room.floorGuideFractions) {
        const float y = room.wallBottomY + fraction * fraction
                        * (World::height - room.wallBottomY);
        SDL_RenderLine(renderer_, 0.0F, y, static_cast<float>(World::width), y);
    }
    SDL_SetRenderDrawColor(renderer_, room.wallTrimColor.r, room.wallTrimColor.g,
                           room.wallTrimColor.b, room.wallTrimColor.a);
    SDL_RenderLine(renderer_, 0.0F, room.wallBottomY,
                   static_cast<float>(World::width), room.wallBottomY);

    const SDL_FPoint feet = game.player().interpolatedFeet(interpolation);
    SDL_FPoint segmentStart = feet;
    SDL_SetRenderDrawColor(renderer_, 237, 224, 157, 255);
    for (const SDL_FPoint waypoint : game.player().remainingPath()) {
        SDL_RenderLine(renderer_, segmentStart.x, segmentStart.y,
                       waypoint.x, waypoint.y);
        segmentStart = waypoint;
    }
    const auto& scenery = room.scenery;
    struct DrawItem { float depth; int objectIndex; };
    std::vector<DrawItem> items;
    items.reserve(scenery.size() + 1);
    for (std::size_t i = 0; i < scenery.size(); ++i) {
        items.push_back({scenery[i].depth(), static_cast<int>(i)});
    }
    items.push_back({feet.y, -1});
    std::stable_sort(items.begin(), items.end(), [](const DrawItem& a, const DrawItem& b) {
        return a.depth < b.depth;
    });
    for (const DrawItem item : items) {
        if (item.objectIndex < 0) {
            drawPlayer(renderer_, room, feet);
        } else {
            drawObject(renderer_, scenery[static_cast<std::size_t>(item.objectIndex)]);
        }
    }

    const SDL_FPoint destination = game.player().destination();
    if (game.player().moving()) {
        SDL_SetRenderDrawColor(renderer_, 246, 206, 124, 255);
        SDL_RenderLine(renderer_, destination.x - 7.0F, destination.y,
                       destination.x + 7.0F, destination.y);
        SDL_RenderLine(renderer_, destination.x, destination.y - 7.0F,
                       destination.x, destination.y + 7.0F);
    }

    const FloatingText& floating = game.floatingText();
    if (floating.isVisible()) {
        SDL_SetRenderDrawColor(renderer_, 240, 220, 100, 255);
        SDL_SetRenderScale(renderer_, 4.0F, 4.0F);
        SDL_RenderDebugText(renderer_, floating.interpolatedX(interpolation) / 4.0F,
                            floating.y() / 4.0F, floating.text().c_str());
        SDL_SetRenderScale(renderer_, 1.0F, 1.0F);
    }

    int windowWidth = 0;
    int windowHeight = 0;
    int outputWidth = 0;
    int outputHeight = 0;
    SDL_FRect viewport{};
    SDL_GetWindowSize(window_, &windowWidth, &windowHeight);
    SDL_GetRenderOutputSize(renderer_, &outputWidth, &outputHeight);
    SDL_GetRenderLogicalPresentationRect(renderer_, &viewport);

    const SDL_FPoint panel = anchored(Anchor::topLeft, 444.0F, 176.0F, 12.0F);
    const SDL_FRect panelRect{panel.x, panel.y, 444.0F, 176.0F};
    SDL_SetRenderDrawColor(renderer_, 9, 12, 19, 230);
    SDL_RenderFillRect(renderer_, &panelRect);
    SDL_SetRenderDrawColor(renderer_, 198, 215, 231, 255);
    SDL_RenderDebugText(renderer_, panel.x + 10.0F, panel.y + 10.0F,
                        "POINT & CLICK  |  left click: move");
    SDL_RenderDebugTextFormat(renderer_, panel.x + 10.0F, panel.y + 28.0F,
                              "player feet: (%.1f, %.1f)  %s", feet.x, feet.y,
                              game.player().moving() ? "moving" : "idle");
    SDL_RenderDebugTextFormat(renderer_, panel.x + 10.0F, panel.y + 44.0F,
                              "destination: (%.1f, %.1f)  route: %zu",
                              destination.x, destination.y, game.player().remainingWaypoints());
    SDL_RenderDebugTextFormat(renderer_, panel.x + 10.0F, panel.y + 60.0F,
                              "mouse world: (%.1f, %.1f)  %s",
                              game.pointer().x, game.pointer().y,
                              game.pointerInside() ? "inside" : "outside");
    SDL_RenderDebugTextFormat(renderer_, panel.x + 10.0F, panel.y + 76.0F,
                              "window: %d x %d  output: %d x %d",
                              windowWidth, windowHeight, outputWidth, outputHeight);
    SDL_RenderDebugTextFormat(renderer_, panel.x + 10.0F, panel.y + 92.0F,
                              "world: %d x %d  viewport: %.0f x %.0f",
                              World::width, World::height, viewport.w, viewport.h);
    SDL_RenderDebugTextFormat(renderer_, panel.x + 10.0F, panel.y + 108.0F,
                              "viewport offset: (%.0f, %.0f)  scale: %.2f",
                              viewport.x, viewport.y, viewport.w / World::width);
    SDL_RenderDebugTextFormat(renderer_, panel.x + 10.0F, panel.y + 124.0F,
                              "wall: y < %.0f  player depth scale: %.2f",
                              room.wallBottomY, room.scaleAt(feet.y));
    SDL_RenderDebugTextFormat(renderer_, panel.x + 10.0F, panel.y + 140.0F,
                              "speed: %.1f px/s  far: %.0f  near: %.0f",
                              room.speedAt(feet.y), room.playerFarSpeed,
                              room.playerNearSpeed);

    const SDL_FPoint hint = anchored(Anchor::bottomRight, 240.0F, 18.0F, 12.0F);
    SDL_SetRenderDrawColor(renderer_, 160, 175, 190, 255);
    SDL_RenderDebugText(renderer_, hint.x, hint.y, "type to show floating text");

    SDL_RenderPresent(renderer_);
}

void Renderer::shutdown() {
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    window_ = nullptr;
}
