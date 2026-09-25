#include "content/RoomFiles.hpp"
#include "game/Game.hpp"
#include "graphics/SpriteLibrary.hpp"

#include <SDL3/SDL.h>
#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>

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

    Room room = Room::firstRoom();
    room.playerSprites.left = clip;
    room.playerSprites.right = SpriteClip{.file = "player_direita.png", .frameWidth = 4,
                                           .frameHeight = 4, .frames = 2};
    room.playerSprites.up = SpriteClip{.file = "player_cima.png", .frameWidth = 6,
                                        .frameHeight = 8, .frames = 3};
    room.scenery[0].sprite = SpriteClip{.file = "props/flag.png"};
    std::string error;
    const auto path = directory / "sprite_room.room";
    assert(RoomFiles::save(path, room, error));
    const auto loaded = RoomFiles::load(path);
    assert(loaded.room);
    assert(loaded.room->playerSprites.left.frameWidth == 4);
    assert(loaded.room->playerSprites.left.frames == 4);
    assert(loaded.room->playerSprites.right.file == "player_direita.png");
    assert(loaded.room->playerSprites.right.frames == 2);
    assert(loaded.room->playerSprites.up.frameWidth == 6);
    assert(loaded.room->playerSprites.up.frameHeight == 8);
    assert(loaded.room->scenery[0].sprite->file == "props/flag.png");
    assert(loaded.room->scenery[0].sprite->frameWidth == 0);
    Game game(*loaded.room);
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

    room.playerSprites.left.frameHeight = 0;
    assert(!RoomFiles::save(path, room, error));
    assert(RoomFiles::load(path).room->playerSprites.left.frameHeight == 4);
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

void testPlayerArtwork() {
    const auto assets = std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets";
    const auto loaded = RoomFiles::load(assets / "rooms" / "first.room");
    assert(loaded.room);
    const auto& clips = loaded.room->playerSprites;
    const SpriteClip* animations[]{&clips.idle, &clips.left, &clips.right,
                                    &clips.up, &clips.down};
    assert(clips.idle.frames == 8 && clips.idle.columns == 1);
    for (const auto* clip : animations) {
        assert(clip->frameWidth == 32 && clip->frameHeight == 48);
        assert(clip->frames == (clip == &clips.idle ? 8 : 7));
        assert(clip->columns == (clip == &clips.idle ? 1 : 7));
        assert(clip->spacingX == (clip == &clips.idle ? 0 : 1));
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
    testPlayerArtwork();
    std::filesystem::remove_all(directory);
}
