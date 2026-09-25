#include "content/RoomFiles.hpp"
#include "content/RoomPresets.hpp"
#include "game/Game.hpp"
#include "graphics/EntityRenderer.hpp"
#include "graphics/SpriteLibrary.hpp"

#include <SDL3/SDL.h>
#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace {
SDL_Color pixel(SDL_Surface* surface, const int x, const int y) {
    SDL_Color result{};
    assert(SDL_ReadSurfacePixel(surface, x, y, &result.r, &result.g, &result.b, &result.a));
    return result;
}

void expectColor(const SDL_Color actual, const Uint8 red, const Uint8 green, const Uint8 blue) {
    assert(actual.r == red && actual.g == green && actual.b == blue);
}

void testClipDefinition(const std::filesystem::path& directory) {
    SpriteClip clip{.file = "player_esquerda.png", .frameWidth = 4, .frameHeight = 4,
                    .frames = 4, .columns = 2, .marginX = 1, .marginY = 1,
                    .spacingX = 1, .spacingY = 1, .fps = 2};
    assert(clip.validationError().empty());
    assert(clip.frameAt(0.0F) == 0);
    assert(clip.frameAt(0.6F) == 1);
    assert(clip.frameAt(1.1F) == 2);
    assert(clip.frameAt(2.0F) == 0);
    clip.loop = false;
    assert(clip.frameAt(3.0F) == 3);
    clip.displayWidth = 60;
    clip.displayHeight = 90;
    clip.offsetX = 5;
    const auto target = clip.targetRect({100, 200}, 48, 80, 0.5F);
    assert(target.x == 87.5F && target.y == 155 && target.w == 30 && target.h == 45);

    Room room = RoomPresets::firstRoom();
    room.playerAnimations["left"] = clip;
    room.playerAnimations["right"] = SpriteClip{.file = "player_direita.png", .frameWidth = 4,
                                           .frameHeight = 4, .frames = 2};
    room.playerAnimations["up"] = SpriteClip{.file = "player_cima.png", .frameWidth = 6,
                                        .frameHeight = 8, .frames = 3};
    room.playerAnimations["wave"] = SpriteClip{.file = "player_wave.png", .frameWidth = 8,
                                                 .frameHeight = 8, .frames = 2};
    room.scenery[0].animations["idle"] = SpriteClip{.file = "props/flag.png"};
    room.scenery[0].animations["wave"] = SpriteClip{.file = "props/flag_wave.png",
                                                      .frameWidth = 8, .frameHeight = 8,
                                                      .frames = 3, .loop = false};
    room.scenery[1].animations["idle"] = SpriteClip{.file = "props/red_box.png"};
    room.scenery[1].animations["glow"] = SpriteClip{.file = "props/red_box_glow.png"};
    room.scenery[1].initialAnimation = "glow";
    std::string error;
    const auto path = directory / "sprite_room.room";
    assert(RoomFiles::save(path, room, error));
    const auto loaded = RoomFiles::load(path);
    assert(loaded.room);
    assert(loaded.room->playerAnimations.at("left").frameWidth == 4);
    assert(loaded.room->playerAnimations.at("left").frames == 4);
    assert(loaded.room->playerAnimations.at("right").file == "player_direita.png");
    assert(loaded.room->playerAnimations.at("right").frames == 2);
    assert(loaded.room->playerAnimations.at("up").frameWidth == 6);
    assert(loaded.room->playerAnimations.at("up").frameHeight == 8);
    assert(loaded.room->playerAnimations.at("wave").frames == 2);
    assert(loaded.room->scenery[0].animations.at("idle").file == "props/flag.png");
    assert(loaded.room->scenery[0].animations.at("idle").frameWidth == 0);
    assert(loaded.room->scenery[0].animations.at("wave").frames == 3);
    assert(!loaded.room->scenery[0].animations.at("wave").loop);
    assert(loaded.room->scenery[1].initialAnimation == "glow");
    Game game(*loaded.room);
    assert(game.objectAnimation("blue_box"));
    assert(game.objectAnimation("blue_box")->name() == "idle");
    assert(game.objectAnimation("red_box")->name() == "glow");
    assert(game.playObjectAnimation("blue_box", "wave", false, error));
    game.tick(0.5F);
    assert(game.objectAnimation("blue_box")->name() == "wave");
    assert(game.objectAnimation("blue_box")->elapsed() == 0.5F);
    assert(game.objectAnimation("red_box")->elapsed() == 0.5F);
    assert(game.objectAnimation("blue_box")->finished(
            loaded.room->scenery[0].animations.at("wave")));
    assert(game.playObjectAnimation("blue_box", "wave", true, error));
    assert(game.objectAnimation("blue_box")->elapsed() == 0.0F);
    assert(game.objectAnimation("red_box")->elapsed() == 0.5F);
    assert(!game.playObjectAnimation("blue_box", "missing", false, error));
    assert(game.objectAnimation("blue_box")->name() == "wave");
    assert(game.applyRoom(*loaded.room, false, error));
    assert(game.objectAnimation("blue_box")->elapsed() == 0.0F);
    assert(game.applyRoom(*loaded.room, true, error));
    assert(game.objectAnimation("blue_box")->name() == "idle");
    game.onClick({300, 360});
    game.tick(0.05F);
    assert(game.player().pose() == PlayerPose::left);
    game.onClick({900, 360});
    game.tick(0.05F);
    assert(game.player().pose() == PlayerPose::right);
    game.onClick({game.player().feet().x, 320});
    game.tick(0.05F);
    assert(game.player().pose() == PlayerPose::up);
    game.onClick({game.player().feet().x, 600});
    game.tick(0.05F);
    assert(game.player().pose() == PlayerPose::down);
    for (int i = 0; i < 200; ++i) game.tick(0.05F);
    assert(game.player().pose() == PlayerPose::idle);

    room.playerAnimations["left"].frameHeight = 0;
    assert(!RoomFiles::save(path, room, error));
    assert(RoomFiles::load(path).room->playerAnimations.at("left").frameHeight == 4);

    const auto oldFormat = directory / "legacy.room";
    std::ofstream out(oldFormat);
    out << "[room]\nid = legacy\n[object flag]\nbounds = 10, 10, 40, 40\n"
           "type = decoration\nsprite.file = props/flag.png\n";
    out.close();
    const auto legacy = RoomFiles::load(oldFormat);
    assert(legacy.room);
    assert(legacy.room->scenery[0].animations.at("idle").file == "props/flag.png");
    std::ofstream duplicate(oldFormat, std::ios::app);
    duplicate << "animation.idle.file = props/other.png\n";
    duplicate.close();
    assert(!RoomFiles::load(oldFormat).room);
}

void testPngRendering(const std::filesystem::path& directory) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_Surface* sheet = SDL_CreateSurface(11, 11, SDL_PIXELFORMAT_RGBA32);
    assert(sheet);
    SDL_FillSurfaceRect(sheet, nullptr, SDL_MapSurfaceRGBA(sheet, 0, 0, 0, 0));
    const SDL_Color colors[4]{{250, 20, 20, 255}, {20, 240, 20, 255},
                               {20, 20, 240, 255}, {250, 220, 20, 255}};
    for (int frame = 0; frame < 4; ++frame) {
        const SDL_Rect rect{1 + (frame % 2) * 5, 1 + (frame / 2) * 5, 4, 4};
        assert(SDL_FillSurfaceRect(sheet, &rect, SDL_MapSurfaceRGBA(
                sheet, colors[frame].r, colors[frame].g, colors[frame].b, 255)));
    }
    const SDL_Rect distinctivePixel{6, 1, 1, 4};
    assert(SDL_FillSurfaceRect(sheet, &distinctivePixel,
                               SDL_MapSurfaceRGBA(sheet, 255, 255, 255, 255)));
    const auto file = directory / "sheet.png";
    assert(SDL_SavePNG(sheet, file.string().c_str()));
    SDL_DestroySurface(sheet);

    SDL_Surface* output = SDL_CreateSurface(64, 64, SDL_PIXELFORMAT_RGBA32);
    assert(output);
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(output);
    assert(renderer);
    SpriteLibrary sprites;
    sprites.initialize(renderer);
    sprites.setRoot(directory);
    SpriteClip clip{.file = "sheet.png", .frameWidth = 4, .frameHeight = 4,
                    .frames = 4, .columns = 2, .marginX = 1, .marginY = 1,
                    .spacingX = 1, .spacingY = 1, .fps = 2};
    for (int frame = 0; frame < 4; ++frame) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        assert(sprites.draw(clip, {0, 0, 40, 40}, frame * 0.5F));
        SDL_RenderPresent(renderer);
        const auto color = colors[frame];
        expectColor(pixel(output, 20, 20), color.r, color.g, color.b);
    }
    assert(sprites.draw(clip, {0, 0, 40, 40}, 0.5F));
    SDL_RenderPresent(renderer);
    expectColor(pixel(output, 2, 20), 255, 255, 255);
    expectColor(pixel(output, 37, 20), 20, 240, 20);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    assert(sprites.draw(clip, {0, 0, 40, 40}, 0.5F, true));
    SDL_RenderPresent(renderer);
    expectColor(pixel(output, 20, 20), 20, 240, 20);
    expectColor(pixel(output, 2, 20), 20, 240, 20);
    expectColor(pixel(output, 37, 20), 255, 255, 255);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SpriteClip staticImage{.file = "sheet.png"};
    assert(sprites.draw(staticImage, {0, 0, 44, 44}, 0));
    SDL_RenderPresent(renderer);
    expectColor(pixel(output, 10, 10), 250, 20, 20);
    expectColor(pixel(output, 30, 10), 20, 240, 20);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    clip.columns = 0;
    assert(sprites.draw(clip, {0, 0, 40, 40}, 1.0F));
    SDL_RenderPresent(renderer);
    expectColor(pixel(output, 20, 20), 20, 20, 240);

    SpriteClip intermediateOutside = clip;
    intermediateOutside.columns = 3;
    assert(!sprites.draw(intermediateOutside, {0, 0, 40, 40}, 0));
    SDL_RenderPresent(renderer);
    expectColor(pixel(output, 6, 6), 145, 0, 190);

    SpriteClip invalid = clip;
    invalid.frames = 8; // The PNG contains only four cutouts.
    assert(!sprites.draw(invalid, {0, 0, 40, 40}, 0));
    SDL_RenderPresent(renderer);
    expectColor(pixel(output, 6, 6), 145, 0, 190);
    expectColor(pixel(output, 18, 6), 10, 7, 17);
    SpriteClip missing{.file = "future.png"};
    assert(!sprites.draw(missing, {0, 0, 40, 40}, 0));
    SDL_RenderPresent(renderer);
    expectColor(pixel(output, 6, 6), 145, 0, 190);

    sprites.clear();
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(output);
    SDL_Quit();
}

void testEntityAnimationSelection(const std::filesystem::path& directory) {
    Room room = RoomPresets::firstRoom();
    SceneObject& object = room.scenery[0];
    object.bounds = {0, 0, 40, 40};
    object.animations["idle"] = SpriteClip{.file = "sheet.png", .frameWidth = 4,
                                            .frameHeight = 4, .frames = 1, .columns = 2,
                                            .marginX = 1, .marginY = 1,
                                            .spacingX = 1, .spacingY = 1};
    object.animations["open"] = object.animations.at("idle");
    object.animations["open"].firstFrame = 1;
    Game game(room);

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_Surface* output = SDL_CreateSurface(40, 40, SDL_PIXELFORMAT_RGBA32);
    assert(output);
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(output);
    assert(renderer);
    SpriteLibrary sprites;
    sprites.initialize(renderer);
    sprites.setRoot(directory);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    EntityRenderer::drawObject(renderer, sprites, game.room().scenery[0],
                               game.objectAnimation("blue_box"));
    SDL_RenderPresent(renderer);
    expectColor(pixel(output, 20, 20), 250, 20, 20);

    std::string error;
    assert(game.playObjectAnimation("blue_box", "open", false, error));
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    EntityRenderer::drawObject(renderer, sprites, game.room().scenery[0],
                               game.objectAnimation("blue_box"));
    SDL_RenderPresent(renderer);
    expectColor(pixel(output, 20, 20), 20, 240, 20);
    sprites.clear();
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(output);
    SDL_Quit();
}

void testPlayerArtwork() {
    const auto assets = std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets";
    const auto loaded = RoomFiles::load(assets / "rooms" / "first.room");
    assert(loaded.room);
    const auto& clips = loaded.room->playerAnimations;
    const SpriteClip* animations[]{&clips.at("idle"), &clips.at("left"),
                                    &clips.at("right"), &clips.at("up"), &clips.at("down")};
    assert(clips.at("idle").frames == 8 && clips.at("idle").columns == 1);
    for (const auto* clip : animations) {
        assert(clip->frameWidth == 32 && clip->frameHeight == 48);
        assert(clip->frames == (clip == &clips.at("idle") ? 8 : 7));
        assert(clip->columns == (clip == &clips.at("idle") ? 1 : 7));
        assert(clip->spacingX == (clip == &clips.at("idle") ? 0 : 1));
    }

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_Surface* output = SDL_CreateSurface(64, 96, SDL_PIXELFORMAT_RGBA32);
    assert(output);
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(output);
    assert(renderer);
    SpriteLibrary sprites;
    sprites.initialize(renderer);
    sprites.setRoot(assets / "sprites");
    for (const auto* clip : animations) {
        for (int frame = 0; frame < clip->frames; ++frame) {
            assert(sprites.draw(*clip, {0, 0, 64, 96}, frame / clip->fps));
        }
    }
    sprites.clear();
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(output);
    SDL_Quit();
}
}

int main() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("pointclick-sprites-" + std::to_string(suffix));
    std::filesystem::create_directory(directory);
    testClipDefinition(directory);
    testPngRendering(directory);
    testEntityAnimationSelection(directory);
    testPlayerArtwork();
    std::filesystem::remove_all(directory);
}
