#include "content/ActorFiles.hpp"
#include "content/ArtCheck.hpp"
#include "content/RoomLibrary.hpp"
#include "content/RoomFiles.hpp"
#include "support/RoomPresets.hpp"
#include "game/Game.hpp"
#include "graphics/EntityRenderer.hpp"
#include "graphics/FontLibrary.hpp"
#include "graphics/Renderer.hpp"
#include "graphics/SpriteLibrary.hpp"

#include "support/TempDirectory.hpp"
#include "support/WalkthroughDriver.hpp"

#include <doctest/doctest.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
const AnimationSet& sharedPlayerArt() {
    static const AnimationSet art = [] {
        const auto loaded = ActorFiles::load(std::filesystem::path(POINTCLICK_SOURCE_DIR)
                                             / "assets/actors/player.actor");
        REQUIRE(loaded.animations);
        return *loaded.animations;
    }();
    return art;
}

SDL_Color pixel(SDL_Surface* surface, const int x, const int y) {
    SDL_Color result{};
    REQUIRE(SDL_ReadSurfacePixel(surface, x, y, &result.r, &result.g, &result.b, &result.a));
    return result;
}

void expectColor(const SDL_Color actual, const Uint8 red, const Uint8 green, const Uint8 blue) {
    REQUIRE((actual.r == red && actual.g == green && actual.b == blue));
}

TEST_CASE("testClipDefinition") {
    const TempDirectory testDirectory("pointclick-sprites");
    const std::filesystem::path& directory = testDirectory.path();
    SpriteClip clip{.file = "player_esquerda.png", .frameWidth = 4, .frameHeight = 4,
                    .frames = 4, .columns = 2, .marginX = 1, .marginY = 1,
                    .spacingX = 1, .spacingY = 1, .fps = 2};
    REQUIRE(clip.validationError().empty());
    REQUIRE(clip.frameAt(0.0F) == 0);
    REQUIRE(clip.frameAt(0.6F) == 1);
    REQUIRE(clip.frameAt(1.1F) == 2);
    REQUIRE(clip.frameAt(2.0F) == 0);
    clip.loop = false;
    REQUIRE(clip.frameAt(3.0F) == 3);
    clip.displayWidth = 60;
    clip.displayHeight = 90;
    clip.offsetX = 5;
    const auto target = clip.targetRect({100, 200}, 48, 80, 0.5F);
    REQUIRE((target.x == 87.5F && target.y == 155 && target.w == 30 && target.h == 45));

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
    REQUIRE(RoomFiles::save(path, room, error));
    const auto loaded = RoomFiles::load(path);
    REQUIRE(loaded.room);
    REQUIRE(loaded.room->playerAnimations.at("left").frameWidth == 4);
    REQUIRE(loaded.room->playerAnimations.at("left").frames == 4);
    REQUIRE(loaded.room->playerAnimations.at("right").file == "player_direita.png");
    REQUIRE(loaded.room->playerAnimations.at("right").frames == 2);
    REQUIRE(loaded.room->playerAnimations.at("up").frameWidth == 6);
    REQUIRE(loaded.room->playerAnimations.at("up").frameHeight == 8);
    REQUIRE(loaded.room->playerAnimations.at("wave").frames == 2);
    REQUIRE(loaded.room->scenery[0].animations.at("idle").file == "props/flag.png");
    REQUIRE(loaded.room->scenery[0].animations.at("idle").frameWidth == 0);
    REQUIRE(loaded.room->scenery[0].animations.at("wave").frames == 3);
    REQUIRE(!loaded.room->scenery[0].animations.at("wave").loop);
    REQUIRE(loaded.room->scenery[1].initialAnimation == "glow");
    Game game(*loaded.room);
    REQUIRE(game.scene().objectAnimation("blue_box"));
    REQUIRE(game.scene().objectAnimation("blue_box")->name() == "idle");
    REQUIRE(game.scene().objectAnimation("red_box")->name() == "glow");
    REQUIRE(game.playObjectAnimation("blue_box", "wave", false, error));
    game.tick(0.5F);
    REQUIRE(game.scene().objectAnimation("blue_box")->name() == "wave");
    REQUIRE(game.scene().objectAnimation("blue_box")->elapsed() == 0.5F);
    REQUIRE(game.scene().objectAnimation("red_box")->elapsed() == 0.5F);
    REQUIRE(game.scene()
                    .objectAnimation("blue_box")
                    ->finished(loaded.room->scenery[0].animations.at("wave")));
    REQUIRE(game.playObjectAnimation("blue_box", "wave", true, error));
    REQUIRE(game.scene().objectAnimation("blue_box")->elapsed() == 0.0F);
    REQUIRE(game.scene().objectAnimation("red_box")->elapsed() == 0.5F);
    REQUIRE(!game.playObjectAnimation("blue_box", "missing", false, error));
    REQUIRE(game.scene().objectAnimation("blue_box")->name() == "wave");
    REQUIRE(game.applyRoom(*loaded.room, false, error));
    REQUIRE(game.scene().objectAnimation("blue_box")->elapsed() == 0.0F);
    REQUIRE(game.applyRoom(*loaded.room, true, error));
    REQUIRE(game.scene().objectAnimation("blue_box")->name() == "idle");
    game.onClick({300, 360});
    game.tick(0.05F);
    REQUIRE(game.scene().player().pose() == PlayerPose::left);
    game.onClick({900, 360});
    game.tick(0.05F);
    REQUIRE(game.scene().player().pose() == PlayerPose::right);
    game.onClick({game.scene().player().feet().x, 320});
    game.tick(0.05F);
    REQUIRE(game.scene().player().pose() == PlayerPose::up);
    game.onClick({game.scene().player().feet().x, 600});
    game.tick(0.05F);
    REQUIRE(game.scene().player().pose() == PlayerPose::down);
    for (int i = 0; i < 200; ++i) game.tick(0.05F);
    REQUIRE(game.scene().player().pose() == PlayerPose::idle);

    room.playerAnimations["left"].frameHeight = 0;
    REQUIRE(!RoomFiles::save(path, room, error));
    REQUIRE(RoomFiles::load(path).room->playerAnimations.at("left").frameHeight == 4);

    const auto oldFormat = directory / "legacy_flag.room";
    std::ofstream out(oldFormat);
    out << "[room]\nid = legacy\n[object flag]\nbounds = 10, 10, 40, 40\n"
           "type = decoration\nsprite.file = props/flag.png\n";
    out.close();
    // A room without player clips inherits the shared actor art, like the shipped rooms.
    REQUIRE(!RoomFiles::load(oldFormat).room);
    const auto legacy = RoomFiles::load(oldFormat, &sharedPlayerArt());
    REQUIRE(legacy.room);
    REQUIRE(legacy.room->scenery[0].animations.at("idle").file == "props/flag.png");
    std::ofstream duplicate(oldFormat, std::ios::app);
    duplicate << "animation.idle.file = props/other.png\n";
    duplicate.close();
    REQUIRE(!RoomFiles::load(oldFormat, &sharedPlayerArt()).room);

    // Inherited clips stay out of a saved room; a field the room overrides is kept.
    Room inherits = *legacy.room;
    inherits.playerAnimations["idle"].fps = 3.0F;
    const auto inheritsPath = directory / "inherits.room";
    REQUIRE(RoomFiles::save(inheritsPath, inherits, error, &sharedPlayerArt()));
    std::ifstream saved(inheritsPath);
    const std::string savedText((std::istreambuf_iterator<char>(saved)), {});
    REQUIRE(savedText.find("player.left.") == std::string::npos);
    REQUIRE(savedText.find("player.idle.fps = 3") != std::string::npos);
    const auto reloaded = RoomFiles::load(inheritsPath, &sharedPlayerArt());
    REQUIRE((reloaded.room && reloaded.room->playerAnimations == inherits.playerAnimations));
}

// The 11x11 sheet (four 4x4 frames, 1px margin and spacing) that several cases cut. Each case
// writes its own copy, so any of them runs alone under a doctest filter.
// Frame colours of the sheet, in frame order.
constexpr SDL_Color colors[4]{
    {250, 20, 20, 255}, {20, 240, 20, 255}, {20, 20, 240, 255}, {250, 220, 20, 255}};

void writeSheetPng(const std::filesystem::path& directory) {
    SDL_Surface* sheet = SDL_CreateSurface(11, 11, SDL_PIXELFORMAT_RGBA32);
    REQUIRE(sheet);
    SDL_FillSurfaceRect(sheet, nullptr, SDL_MapSurfaceRGBA(sheet, 0, 0, 0, 0));
    for (int frame = 0; frame < 4; ++frame) {
        const SDL_Rect rect{1 + (frame % 2) * 5, 1 + (frame / 2) * 5, 4, 4};
        REQUIRE(SDL_FillSurfaceRect(
                sheet, &rect,
                SDL_MapSurfaceRGBA(sheet, colors[frame].r, colors[frame].g, colors[frame].b, 255)));
    }
    const SDL_Rect distinctivePixel{6, 1, 1, 4};
    REQUIRE(SDL_FillSurfaceRect(sheet, &distinctivePixel,
                                SDL_MapSurfaceRGBA(sheet, 255, 255, 255, 255)));
    const auto file = directory / "sheet.png";
    REQUIRE(SDL_SavePNG(sheet, file.string().c_str()));
    SDL_DestroySurface(sheet);
}

TEST_CASE("testPngRendering") {
    const TempDirectory testDirectory("pointclick-sprites");
    const std::filesystem::path& directory = testDirectory.path();
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    REQUIRE(SDL_Init(SDL_INIT_VIDEO));
    writeSheetPng(directory);

    SDL_Surface* output = SDL_CreateSurface(64, 64, SDL_PIXELFORMAT_RGBA32);
    REQUIRE(output);
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(output);
    REQUIRE(renderer);
    SpriteLibrary sprites;
    sprites.initialize(renderer);
    sprites.setRoot(directory);
    SpriteClip clip{.file = "sheet.png", .frameWidth = 4, .frameHeight = 4,
                    .frames = 4, .columns = 2, .marginX = 1, .marginY = 1,
                    .spacingX = 1, .spacingY = 1, .fps = 2};
    for (int frame = 0; frame < 4; ++frame) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        REQUIRE(sprites.draw(clip, {0, 0, 40, 40}, static_cast<float>(frame) * 0.5F));
        SDL_RenderPresent(renderer);
        const auto color = colors[frame];
        expectColor(pixel(output, 20, 20), color.r, color.g, color.b);
    }
    REQUIRE(sprites.draw(clip, {0, 0, 40, 40}, 0.5F));
    SDL_RenderPresent(renderer);
    expectColor(pixel(output, 2, 20), 255, 255, 255);
    expectColor(pixel(output, 37, 20), 20, 240, 20);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    REQUIRE(sprites.draw(clip, {0, 0, 40, 40}, 0.5F, true));
    SDL_RenderPresent(renderer);
    expectColor(pixel(output, 20, 20), 20, 240, 20);
    expectColor(pixel(output, 2, 20), 20, 240, 20);
    expectColor(pixel(output, 37, 20), 255, 255, 255);

    // flip_x on the clip mirrors the same way; with the caller's flip they cancel out.
    SpriteClip mirrored = clip;
    mirrored.flipX = true;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    REQUIRE(sprites.draw(mirrored, {0, 0, 40, 40}, 0.5F));
    SDL_RenderPresent(renderer);
    expectColor(pixel(output, 2, 20), 20, 240, 20);
    expectColor(pixel(output, 37, 20), 255, 255, 255);
    SDL_RenderClear(renderer);
    REQUIRE(sprites.draw(mirrored, {0, 0, 40, 40}, 0.5F, true));
    SDL_RenderPresent(renderer);
    expectColor(pixel(output, 2, 20), 255, 255, 255);
    expectColor(pixel(output, 37, 20), 20, 240, 20);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SpriteClip staticImage{.file = "sheet.png"};
    REQUIRE(sprites.draw(staticImage, {0, 0, 44, 44}, 0));
    SDL_RenderPresent(renderer);
    expectColor(pixel(output, 10, 10), 250, 20, 20);
    expectColor(pixel(output, 30, 10), 20, 240, 20);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    clip.columns = 0;
    REQUIRE(sprites.draw(clip, {0, 0, 40, 40}, 1.0F));
    SDL_RenderPresent(renderer);
    expectColor(pixel(output, 20, 20), 20, 20, 240);

    SpriteClip intermediateOutside = clip;
    intermediateOutside.columns = 3;
    REQUIRE(!sprites.draw(intermediateOutside, {0, 0, 40, 40}, 0));
    SDL_RenderPresent(renderer);
    expectColor(pixel(output, 6, 6), 145, 0, 190);

    SpriteClip invalid = clip;
    invalid.frames = 8; // The PNG contains only four cutouts.
    REQUIRE(!sprites.draw(invalid, {0, 0, 40, 40}, 0));
    SDL_RenderPresent(renderer);
    expectColor(pixel(output, 6, 6), 145, 0, 190);
    expectColor(pixel(output, 18, 6), 10, 7, 17);
    SpriteClip missing{.file = "future.png"};
    REQUIRE(!sprites.draw(missing, {0, 0, 40, 40}, 0));
    SDL_RenderPresent(renderer);
    expectColor(pixel(output, 6, 6), 145, 0, 190);

    // Room changes keep what the current and previous rooms used; only stale files go.
    REQUIRE(sprites.cachedImageCount() == 2); // sheet.png + the failed future.png
    sprites.beginRoom();
    sprites.beginRoom();
    REQUIRE(sprites.cachedImageCount() == 2); // A -> B -> A must not reload
    REQUIRE(sprites.draw(clip, {0, 0, 40, 40}, 0));
    sprites.beginRoom();
    REQUIRE(sprites.cachedImageCount() == 1); // future.png unused for two room changes

    // A PNG changed on disk keeps its cached texture until the cache is cleared, which is what
    // the editor's `load` and `reload` (and a render device reset) ask for.
    const auto repaintedFile = directory / "repainted.png";
    const auto paint = [&](const Uint8 red, const Uint8 green, const Uint8 blue) {
        SDL_Surface* image = SDL_CreateSurface(8, 8, SDL_PIXELFORMAT_RGBA32);
        REQUIRE(image);
        REQUIRE(SDL_FillSurfaceRect(image, nullptr,
                                    SDL_MapSurfaceRGBA(image, red, green, blue, 255)));
        REQUIRE(SDL_SavePNG(image, repaintedFile.string().c_str()));
        SDL_DestroySurface(image);
    };
    const SpriteClip repainted{.file = "repainted.png"};
    const auto drawn = [&] {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        REQUIRE(sprites.draw(repainted, {0, 0, 40, 40}, 0));
        SDL_RenderPresent(renderer);
        return pixel(output, 20, 20);
    };
    paint(250, 20, 20);
    expectColor(drawn(), 250, 20, 20);
    paint(12, 200, 180);
    expectColor(drawn(), 250, 20, 20); // still the cached texture
    sprites.clear();
    expectColor(drawn(), 12, 200, 180);

    sprites.clear();
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(output);
    SDL_Quit();
}

TEST_CASE("testEntityAnimationSelection") {
    const TempDirectory testDirectory("pointclick-sprites");
    const std::filesystem::path& directory = testDirectory.path();
    writeSheetPng(directory);
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
    REQUIRE(SDL_Init(SDL_INIT_VIDEO));
    SDL_Surface* output = SDL_CreateSurface(40, 40, SDL_PIXELFORMAT_RGBA32);
    REQUIRE(output);
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(output);
    REQUIRE(renderer);
    SpriteLibrary sprites;
    sprites.initialize(renderer);
    sprites.setRoot(directory);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    EntityRenderer::drawObject(renderer, sprites, game.scene().room().scenery[0],
                               game.scene().objectAnimation("blue_box"));
    SDL_RenderPresent(renderer);
    expectColor(pixel(output, 20, 20), 250, 20, 20);

    std::string error;
    REQUIRE(game.playObjectAnimation("blue_box", "open", false, error));
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    EntityRenderer::drawObject(renderer, sprites, game.scene().room().scenery[0],
                               game.scene().objectAnimation("blue_box"));
    SDL_RenderPresent(renderer);
    expectColor(pixel(output, 20, 20), 20, 240, 20);
    sprites.clear();
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(output);
    SDL_Quit();
}

TEST_CASE("testPlayerArtwork") {
    const auto assets = std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets";
    const auto art = ActorFiles::load(assets / "actors" / "player.actor");
    REQUIRE(art.animations);
    const auto loaded = RoomFiles::load(assets / "rooms" / "first.room", &*art.animations);
    REQUIRE(loaded.room);
    const auto& clips = loaded.room->playerAnimations;
    const SpriteClip* animations[]{&clips.at("idle"), &clips.at("left"),
                                    &clips.at("right"), &clips.at("up"), &clips.at("down")};
    REQUIRE((clips.at("idle").frames == 8 && clips.at("idle").columns == 1));
    for (const auto* clip : animations) {
        REQUIRE((clip->frameWidth == 32 && clip->frameHeight == 48));
        REQUIRE(clip->frames == (clip == &clips.at("idle") ? 8 : 7));
        REQUIRE(clip->columns == (clip == &clips.at("idle") ? 1 : 7));
        REQUIRE(clip->spacingX == (clip == &clips.at("idle") ? 0 : 1));
    }

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    REQUIRE(SDL_Init(SDL_INIT_VIDEO));
    SDL_Surface* output = SDL_CreateSurface(64, 96, SDL_PIXELFORMAT_RGBA32);
    REQUIRE(output);
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(output);
    REQUIRE(renderer);
    SpriteLibrary sprites;
    sprites.initialize(renderer);
    sprites.setRoot(assets / "sprites");
    for (const auto* clip : animations) {
        for (int frame = 0; frame < clip->frames; ++frame) {
            REQUIRE(sprites.draw(*clip, {0, 0, 64, 96}, static_cast<float>(frame) / clip->fps));
        }
    }
    sprites.clear();
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(output);
    SDL_Quit();
}
}

// graphics/ alone (a build without tools) draws the game and nothing else: no route, destination
// cross, technical readout, editor hint or collision base of a solid object. Those live in
// debug/, behind the editor.
TEST_CASE("testGameFrameHasNoDebugDrawing") {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    REQUIRE(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window* window = SDL_CreateWindow("Game frame test", 1280, 720, SDL_WINDOW_HIDDEN);
    REQUIRE(window);
    Renderer renderer;
    REQUIRE(renderer.initialize(window));
    renderer.setSpriteRoot(std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets/sprites");
    Game game(RoomPresets::firstRoom());
    game.onClick({900.0F, 600.0F});
    game.tick(0.05F);
    REQUIRE(game.scene().player().moving());
    // red_box: solid, with a collision base and no sprite, so it is drawn as a placeholder.
    const SceneObject& solid = game.scene().room().scenery[1];
    REQUIRE((game.scene().isObjectSolid(solid) && !game.scene().objectAnimation(solid.id)));
    REQUIRE((solid.collisionFootprint.w > 0.0F && solid.collisionFootprint.h > 0.0F));
    renderer.render(game, 1.0F);
    SDL_Surface* frame = SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr);
    REQUIRE((frame && frame->w == 1280 && frame->h == 720));
    const SDL_Color debugColors[]{
        {130, 42, 39, 255},   // collision base of a solid object (D10)
        {237, 224, 157, 255}, // route
        {246, 206, 124, 255}, // destination cross
        {9, 12, 19, 255},     // readout panel
        {198, 215, 231, 255}, // readout text
        {160, 175, 190, 255}, // editor hint
    };
    for (int y = 0; y < frame->h; ++y) {
        for (int x = 0; x < frame->w; ++x) {
            const SDL_Color color = pixel(frame, x, y);
            for (const SDL_Color debug : debugColors) {
                REQUIRE((color.r != debug.r || color.g != debug.g || color.b != debug.b));
            }
        }
    }
    expectColor(pixel(frame, 640, 100), 171, 62, 67); // the wall: the scene itself was drawn
    // The placeholder is one flat rectangle: where the base sits it has the object's own color.
    const SDL_FRect base = solid.worldCollisionFootprint();
    expectColor(pixel(frame, static_cast<int>(base.x + base.w * 0.5F),
                      static_cast<int>(base.y + base.h * 0.5F)),
                solid.color.r, solid.color.g, solid.color.b);
    SDL_DestroySurface(frame);
    renderer.shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
}

// F3: the door playground, played from the shipped files and drawn headless. The twin door uses
// the same sheet as the first with flip_x, so the blue hinge post that sits on the left of the
// first door is on the right of the twin, both closed and open.
TEST_CASE("testDoorPlaygroundFlipPixels") {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    REQUIRE(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window* window = SDL_CreateWindow("Door playground", 1280, 720, SDL_WINDOW_HIDDEN);
    REQUIRE(window);
    Renderer renderer;
    REQUIRE(renderer.initialize(window));
    renderer.setSpriteRoot(std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets/sprites");

    constexpr SDL_Color hinge{40, 60, 200, 255};
    constexpr SDL_Color doorway{25, 25, 40, 255};
    WalkthroughDriver play("door", "entry");
    const SceneObject& door = *play.game().scene().room().findObject("door");
    const SceneObject& twin = *play.game().scene().room().findObject("door_twin");
    // Eight pixels in from each side, halfway up: clear of the player, who stands at the base.
    const auto sample = [&](const SceneObject& object, const bool rightSide) {
        const auto x = static_cast<int>(object.bounds.x
                                        + (rightSide ? object.bounds.w - 8.0F : 8.0F));
        const auto y = static_cast<int>(object.bounds.y + object.bounds.h * 0.5F);
        renderer.render(play.game(), 1.0F);
        SDL_Surface* frame = SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr);
        REQUIRE(frame);
        const SDL_Color color = pixel(frame, x, y);
        SDL_DestroySurface(frame);
        return color;
    };
    const auto expectSide = [&](const SceneObject& object, const bool rightSide,
                                const SDL_Color expected) {
        const SDL_Color color = sample(object, rightSide);
        const std::string where = object.id + (rightSide ? " right side got " : " left side got ")
                                  + std::to_string(color.r) + "," + std::to_string(color.g) + ","
                                  + std::to_string(color.b);
        INFO(where);
        expectColor(color, expected.r, expected.g, expected.b);
    };

    // Closed: wood in the middle, the hinge post on the first door's left and the twin's right.
    expectSide(door, false, hinge);
    expectSide(twin, true, hinge);

    // Open (the last frame of the sheet): the leaf is swung away, the doorway shows behind it.
    play.clickObject("door").settle().advance(15.0F);
    play.clickObject("door_twin").settle().advance(15.0F); // the line is gone from the screen
    REQUIRE(!play.game().scene().floatingText().isVisible());
    play.expectObjectAnimation("door", "open", true)
            .expectObjectAnimation("door_twin", "open", true);
    expectSide(door, false, hinge);
    expectSide(door, true, doorway);
    expectSide(twin, true, hinge);
    expectSide(twin, false, doorway);

    renderer.shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
}

// F8: art the game can run without is a warning, found before any frame.
TEST_CASE("testArtWarnings") {
    const TempDirectory testDirectory("pointclick-sprites");
    const std::filesystem::path& directory = testDirectory.path();
    writeSheetPng(directory);
    // The header reader agrees with a PNG that SDL wrote, and tells missing from unreadable.
    const ArtCheck::PngHeader sheet = ArtCheck::readPngHeader(directory / "sheet.png");
    REQUIRE(sheet.status == ArtCheck::PngStatus::ok);
    REQUIRE((sheet.size.width == 11 && sheet.size.height == 11));
    REQUIRE(ArtCheck::readPngHeader(directory / "nope.png").status == ArtCheck::PngStatus::missing);
    {
        std::ofstream(directory / "text.png", std::ios::binary)
                << "isto nao e um PNG, nem de longe";
    }
    { std::ofstream(directory / "short.png", std::ios::binary) << "\x89PNG"; }
    REQUIRE(ArtCheck::readPngHeader(directory / "text.png").status
            == ArtCheck::PngStatus::unreadable);
    REQUIRE(ArtCheck::readPngHeader(directory / "short.png").status
            == ArtCheck::PngStatus::unreadable);

    // The pure fit check, shared with SpriteLibrary::draw (same cases as testPngRendering).
    const SpriteClip fits{.file = "sheet.png",
                          .frameWidth = 4,
                          .frameHeight = 4,
                          .frames = 4,
                          .columns = 2,
                          .marginX = 1,
                          .marginY = 1,
                          .spacingX = 1,
                          .spacingY = 1};
    SpriteClip tooMany = fits;
    tooMany.frames = 8;
    SpriteClip derived = fits;
    derived.columns = 0;
    REQUIRE((clipFitsImage(fits, 11, 11) && !clipFitsImage(tooMany, 11, 11)));
    REQUIRE((spriteSheetColumns(derived, 11) == 2 && clipFitsImage(derived, 11, 11)));
    REQUIRE((clipFitsImage(fits, 10, 10) && !clipFitsImage(fits, 9, 11)
             && !clipFitsImage(fits, 11, 9)));
    REQUIRE(!clipFitsImage(derived, 3, 11));                       // no column fits
    REQUIRE(clipFitsImage(SpriteClip{.file = "sheet.png"}, 1, 1)); // whole image

    Room room = RoomPresets::firstRoom();
    room.playerAnimations = {
        {"idle", fits}, {"left", fits}, {"right", fits}, {"up", fits}, {"down", fits}};
    room.scenery[0].animations = {{"idle", fits}};
    REQUIRE(room.validationError().empty());
    REQUIRE(ArtCheck::warnings(room, directory).empty());

    const std::string object = room.scenery[0].id;
    room.scenery[0].animations = {{"idle", SpriteClip{.file = "future.png"}},
                                  {"open", SpriteClip{.file = "future.png"}}, // same file: once
                                  {"shut", tooMany},
                                  {"torn", SpriteClip{.file = "text.png"}},
                                  {"void", SpriteClip{}}};
    REQUIRE(room.validationError().empty()); // none of this refuses the room
    const std::vector<ArtWarning> found = ArtCheck::warnings(room, directory);
    REQUIRE((found
             == std::vector<ArtWarning>{
                 {"sprites/future.png", "PNG ausente"},
                 {"sprites/sheet.png", "Corte de " + object + ".shut nao cabe na imagem 11x11"},
                 {"sprites/text.png", "PNG ilegivel"},
                 {"sprites", "Clip " + object + ".void sem arquivo PNG"}}));

    // Fonts: the list comes from FontLibrary, the files from <assets>/fonts.
    const auto fonts = FontLibrary::files();
    const auto assets = std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets";
    REQUIRE(ArtCheck::warnings(Room{}, {}, assets / "fonts", fonts).empty());
    const std::vector<ArtWarning> noFonts = ArtCheck::warnings(Room{}, {}, directory, fonts);
    REQUIRE(noFonts.size() == 2); // two files serve the four roles
    REQUIRE((noFonts[0].message == "Fonte ausente" && noFonts[0].file.starts_with("fonts/")));
}

// F8: a room whose art is missing loads as any other and its frame shows the checkerboard.
TEST_CASE("testRoomWithMissingArtStillRuns") {
    RoomLibrary library;
    std::string error;
    REQUIRE(library.open(std::filesystem::path(POINTCLICK_SOURCE_DIR)
                                 / "tests/fixtures/missing_art/rooms",
                         error));
    RoomBundleResult loaded = library.loadBundle(RoomLibrary::startRoomFile);
    REQUIRE((loaded.bundle && loaded.error.empty()));
    REQUIRE(ArtCheck::warnings(loaded.bundle->room, library.spritesDirectory()).size() == 2);

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    REQUIRE(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window* window = SDL_CreateWindow("Missing art test", 1280, 720, SDL_WINDOW_HIDDEN);
    REQUIRE(window);
    Renderer renderer;
    REQUIRE(renderer.initialize(window));
    renderer.setSpriteRoot(library.spritesDirectory());
    Game game(std::move(loaded.bundle->room));
    game.tick(0.05F);
    renderer.render(game, 1.0F);
    SDL_Surface* frame = SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr);
    REQUIRE((frame && frame->w == 1280 && frame->h == 720));
    // ghost: bounds 600,400 96x96, tiles of 12 px starting purple at its top-left corner.
    expectColor(pixel(frame, 606, 406), 145, 0, 190);
    expectColor(pixel(frame, 618, 406), 10, 7, 17);
    expectColor(pixel(frame, 640, 100), 171, 62, 67); // and the rest of the scene is there
    SDL_DestroySurface(frame);
    renderer.shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
}

// Counts pixels of the speech fill color (250, 236, 170) and of its dark outline in a frame.
struct SpeechPixels {
    int fill = 0;
    int outline = 0;
    SDL_Rect bounds{1280, 720, -1, -1}; // min x, min y, max x, max y of the fill
};

SpeechPixels speechPixels(SDL_Surface* frame) {
    SpeechPixels result;
    for (int y = 0; y < frame->h; ++y) {
        for (int x = 0; x < frame->w; ++x) {
            const SDL_Color c = pixel(frame, x, y);
            if (c.r >= 245 && c.g >= 230 && c.g <= 242 && c.b >= 160 && c.b <= 180) {
                ++result.fill;
                result.bounds.x = std::min(result.bounds.x, x);
                result.bounds.y = std::min(result.bounds.y, y);
                result.bounds.w = std::max(result.bounds.w, x);
                result.bounds.h = std::max(result.bounds.h, y);
            } else if (c.r == 10 && c.g == 12 && c.b == 18) {
                ++result.outline;
            }
        }
    }
    return result;
}

// A long accented line, drawn with the real TTF fonts (and again with the debug-font fallback):
// it shows, wraps inside the viewport and sits above the player.
TEST_CASE("testSpeechUsesTtfText") {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    REQUIRE(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window* window = SDL_CreateWindow("Speech test", 1280, 720, SDL_WINDOW_HIDDEN);
    REQUIRE(window);
    const std::string line = "Encontrei uma chave de latão escondida na caixa azul! Será que "
                             "ela abre a porta do salão? Não custa tentar, já que ninguém viu.";
    const auto render = [&](Renderer& renderer, const bool fonts) {
        Game game(RoomPresets::firstRoom());
        game.setDialogues(RoomDialogues{{DialogueNode{
                "fala", {InteractionEffect::make("say", {line})}, {}}}});
        REQUIRE(game.startDialogue("fala"));
        game.tick(0.016F);
        REQUIRE(game.scene().floatingText().isVisible());
        const TextMetrics& metrics = renderer.textMetrics();
        const float width = metrics.width(FontRole::dialogue, line);
        REQUIRE(width > 1280.0F); // one line would not fit: it must wrap
        if (fonts) {
            // Proportional font: "i" is narrower than "M", unlike the debug font.
            REQUIRE(metrics.width(FontRole::dialogue, "iiii")
                    < metrics.width(FontRole::dialogue, "MMMM"));
            REQUIRE(metrics.lineHeight(FontRole::dialogue) > 30.0F);
        }
        renderer.render(game, 1.0F);
        SDL_Surface* frame = SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr);
        REQUIRE(frame);
        const SpeechPixels found = speechPixels(frame);
        SDL_DestroySurface(frame);
        REQUIRE((found.fill > 200 && found.outline > 100));
        REQUIRE((found.bounds.x >= 8 && found.bounds.w <= 1272 && found.bounds.y >= 8));
        // Several lines tall, and entirely above the player's feet.
        REQUIRE(static_cast<float>(found.bounds.h - found.bounds.y)
                > metrics.lineHeight(FontRole::dialogue) * 1.5F);
        REQUIRE(static_cast<float>(found.bounds.h) < game.scene().player().feet().y);
    };
    {
        Renderer renderer;
        REQUIRE(renderer.initialize(window));
        renderer.setSpriteRoot(std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets/sprites");
        REQUIRE(renderer.setFontRoot(std::filesystem::path(POINTCLICK_SOURCE_DIR)
                                     / "assets/fonts"));
        render(renderer, true);
        // Device reset drops the glyph atlases; the next frame draws again.
        renderer.invalidateTextures();
        render(renderer, true);
        renderer.shutdown();
    }
    {
        Renderer renderer;
        REQUIRE(renderer.initialize(window));
        // Missing fonts: logged, the game text falls back to the debug font and still shows.
        REQUIRE(!renderer.setFontRoot(std::filesystem::path(POINTCLICK_SOURCE_DIR) / "no-fonts"));
        render(renderer, false);
        renderer.shutdown();
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
}
