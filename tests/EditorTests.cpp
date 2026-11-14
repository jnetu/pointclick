#include "content/DialogueFiles.hpp"
#include "content/InteractionFiles.hpp"
#include "content/RoomFiles.hpp"
#include "support/RoomPresets.hpp"
#include "game/ActionCatalog.hpp"
#include "game/RoomParameters.hpp"
#include "content/ActorFiles.hpp"
#include "content/RoomLibrary.hpp"
#include "content/TextFormat.hpp"
#include "debug/CommandCatalog.hpp"
#include "debug/CommandConsole.hpp"
#include "debug/CommandScript.hpp"
#include "debug/DebugEditor.hpp"
#include "debug/DebugOverlay.hpp"
#include "game/Identifier.hpp"
#include "game/Game.hpp"
#include "graphics/Renderer.hpp"
#include "input/InputRouter.hpp"
#include "input/InputSystem.hpp"
#include "ui/DialogueMenu.hpp"

#include "support/TempDirectory.hpp"

#include <doctest/doctest.h>
#include <SDL3/SDL.h>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <stdexcept>
#include <variant>

namespace {
// SDL's sentinel ids are C-cast macros: read them once so the file builds under -Wold-style-cast.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
constexpr SDL_MouseID kTouchMouseId = SDL_TOUCH_MOUSEID;
constexpr SDL_TouchID kMouseTouchId = SDL_MOUSE_TOUCHID;
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// The shipped assets declare player art once, in assets/actors/player.actor.
const AnimationSet* sharedPlayerArt() {
    static const AnimationSet art = [] {
        const auto loaded = ActorFiles::load(std::filesystem::path(POINTCLICK_SOURCE_DIR)
                                             / "assets/actors/player.actor");
        REQUIRE(loaded.animations);
        return *loaded.animations;
    }();
    return &art;
}

RoomLibrary openLibrary(const std::filesystem::path& roomsDirectory) {
    RoomLibrary library;
    std::string error;
    REQUIRE(library.open(roomsDirectory, error));
    return library;
}

void command(DebugEditor& editor, Game& game, const std::string_view text) {
    REQUIRE(editor.onTextInput(text, game));
    REQUIRE(editor.onToolAction(ToolAction::submit, game));
}

TEST_CASE("testEditing") {
    const TempDirectory testDirectory("pointclick-editor-tests");
    const std::filesystem::path& directory = testDirectory.path();
    Game game(RoomPresets::firstRoom());
    DebugEditor editor;
    const RoomLibrary library = openLibrary(directory);
    editor.setLibrary(library);
    int artReloads = 0;
    editor.setArtReloader([&artReloads] { ++artReloads; });
    REQUIRE(!editor.onToolAction(ToolAction::increase, game));
    REQUIRE(!editor.onTextInput("DE", game));
    REQUIRE(!editor.active());
    REQUIRE(editor.onTextInput("bug", game));
    REQUIRE(editor.active());
    editor.onToolAction(ToolAction::increase, game);
    REQUIRE(game.scene().room().playerFarSpeed == 120);
    command(editor, game, "set player.near_speed 400");
    REQUIRE(game.scene().room().playerNearSpeed == 400);

    game.onClick({700, 400});
    const auto before = game.scene().player().destination();
    command(editor, game, "set perspective.near_y 100");
    REQUIRE(game.scene().room().nearDepthY == 720);
    REQUIRE(game.scene().player().moving()); // Rejected changes preserve the whole simulation.
    REQUIRE(game.scene().player().destination().x == before.x);
    command(editor, game, "set player.width -1");
    REQUIRE(game.scene().room().playerBaseSize.x == 48);
    command(editor, game, "set player.width nan");
    REQUIRE(game.scene().room().playerBaseSize.x == 48);

    command(editor, game, "save authored.room");
    command(editor, game, "set player.near_speed 500");
    command(editor, game, "undo");
    REQUIRE(game.scene().room().playerNearSpeed == 400);
    REQUIRE(artReloads == 0); // edits, save and undo keep the texture cache
    command(editor, game, "load authored.room");
    REQUIRE(game.scene().room().playerFarSpeed == 120);
    REQUIRE(!game.scene().player().moving());
    // `load` also asks for the PNGs again (art edited on disk); `reload` does only that.
    REQUIRE(artReloads == 1);
    const SDL_FPoint feetBeforeReload = game.scene().player().feet();
    command(editor, game, "reload");
    REQUIRE((artReloads == 2 && editor.status() == "Sprites recarregados do disco"));
    REQUIRE(game.scene().player().feet().x == feetBeforeReload.x);
    command(editor, game, "load nao_existe.room");
    REQUIRE(artReloads == 2); // a refused load reloads nothing
    command(editor, game, "save ../escape.room");
    REQUIRE(editor.status().find("sem pastas") != std::string::npos);

    command(editor, game, "teleport 860 575");
    REQUIRE(game.scene().player().feet().x == 640); // Cannot place feet in the red base.
    command(editor, game, "move red_box 620 240");
    REQUIRE((game.scene().player().feet().x != 640 || game.scene().player().feet().y != 360));
    REQUIRE(game.scene().room().scenery[1].bounds.x == 620);
    command(editor, game, "move red_box 2000 240");
    REQUIRE(game.scene().room().scenery[1].bounds.x == 620);
    command(editor, game, "set room.width 1000");
    REQUIRE(game.scene().room().width == 1280);
    command(editor, game, "set room.height 600");
    REQUIRE(game.scene().room().height == 720);
    command(editor, game, "set room.width 2560");
    REQUIRE(game.scene().room().width == 2560);
    command(editor, game, "move red_box 2000 240");
    REQUIRE(game.scene().room().scenery[1].bounds.x == 2000);
    command(editor, game, "set room.width 1280");
    REQUIRE(game.scene().room().width == 2560); // Cannot shrink past an object at x = 2000.
    command(editor, game, "reset");
    REQUIRE(game.scene().room().width == 1280);
    REQUIRE(game.scene().room().playerFarSpeed == 110);
    REQUIRE(game.scene().room().scenery[1].bounds.x == 810);

    editor.onToolAction(ToolAction::togglePanel, game);
    REQUIRE((editor.active() && !editor.panelVisible()));
    editor.onToolAction(ToolAction::increase, game);
    REQUIRE(game.scene().room().playerFarSpeed == 110);
    REQUIRE(editor.onTextInput("set player.far_speed 500", game));
    REQUIRE(editor.command().empty());
    editor.onToolAction(ToolAction::togglePanel, game);
    REQUIRE(editor.panelVisible());

    editor.onToolAction(ToolAction::close, game);
    REQUIRE(!editor.active());
    REQUIRE(!editor.onToolAction(ToolAction::increase, game));
    REQUIRE(game.scene().room().playerFarSpeed == 110);

    Room blocked = game.scene().room();
    blocked.scenery = {{{0, 0, 1280, 720}, {255, 0, 0, 255}, SceneObjectType::solid,
                       {0, 0, 1280, 720}, "wall", {}}};
    std::string error;
    REQUIRE(!game.applyRoom(blocked, false, error));
    REQUIRE(game.scene().room().scenery.size() == 2);
}

TEST_CASE("testRoomFiles") {
    const TempDirectory testDirectory("pointclick-editor-tests");
    const std::filesystem::path& directory = testDirectory.path();
    const auto source = std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets/rooms/first.room";
    const auto loaded = RoomFiles::load(source, sharedPlayerArt());
    REQUIRE(loaded.room);
    REQUIRE((loaded.room->width == 1280.0F && loaded.room->height == 720.0F));
    REQUIRE(loaded.room->scenery.size() == 2);
    const auto missing = RoomFiles::load(directory / std::filesystem::path(u8"ausente-\u00e7.room"));
    REQUIRE(!missing.room);
    REQUIRE(missing.error == "Nao foi possivel abrir ausente-\xC3\xA7.room");

    const auto unicodeDirectory = directory / std::filesystem::path(u8"salas-a\u00e7\u00e3o");
    std::filesystem::create_directory(unicodeDirectory);
    const auto commentedPath = unicodeDirectory / std::filesystem::path(u8"sala-\u65e5.room");
    {
        std::ofstream out(commentedPath, std::ios::binary);
        out << "# comentario\r\n[room] # secao\r\n\r\nid = commented # id\r\n"
               "player.far_speed = 123.5 # velocidade";
    }
    const auto commented = RoomFiles::load(commentedPath, sharedPlayerArt());
    REQUIRE((commented.room && commented.error.empty()));
    REQUIRE(commented.room->id == "commented");
    REQUIRE(commented.room->playerFarSpeed == 123.5F);

    const auto emptyPath = directory / "empty.room";
    { std::ofstream out(emptyPath, std::ios::binary); }
    const auto empty = RoomFiles::load(emptyPath, sharedPlayerArt());
    REQUIRE((empty.room && empty.error.empty()));
    REQUIRE(empty.room->id == "empty"); // no `id` key: named after the file, not "first"

    const auto nulPath = directory / "nul.room";
    {
        std::ofstream out(nulPath, std::ios::binary);
        out << "[room]\nid = nul_room\n";
        out.put('\0');
        out << '\n';
    }
    const auto nul = RoomFiles::load(nulPath);
    REQUIRE((!nul.room && nul.error == "Linha 3: Esperado chave = valor"));

    const auto gallery = RoomFiles::load(source.parent_path() / "gallery.room", sharedPlayerArt());
    REQUIRE(gallery.room);
    Game other(*gallery.room);
    REQUIRE(other.scene().room().id == "gallery");
    other.onClick({400, 460});
    REQUIRE(other.scene().player().moving());
    Room room = *loaded.room;
    room.id = "other_room";
    room.width = 2560.0F;
    room.height = 1440.0F;
    room.nearDepthY = 1440.0F;
    room.vanishingPointX = 1280.0F;
    room.playerStart = {1800.0F, 920.0F};
    room.playerNearSpeed = 412.25F;
    room.floorColor = {20, 90, 190, 255};
    room.scenery[1].bounds.x = 2100.0F;
    room.scenery[1].bounds.y = 900.0F;
    room.scenery[1].collisionFootprint.h = 21.5F;
    room.scenery[1].type = SceneObjectType::decoration;
    std::string error;
    const auto path = directory / "roundtrip.room";
    // flip_x (F7): absent stays absent on save; set survives the round-trip.
    room.scenery[1].animations["idle"] = SpriteClip{.file = "red_idle.png"};
    room.scenery[1].animations["walk"] = SpriteClip{.file = "red_walk.png"};
    REQUIRE(RoomFiles::save(path, room, error));
    REQUIRE(TextFormat::readFile(path).text->find("flip_x") == std::string::npos);
    room.scenery[1].animations["walk"].flipX = true;
    REQUIRE(RoomFiles::save(path, room, error));
    REQUIRE(TextFormat::readFile(path).text->find("animation.walk.flip_x = 1")
            != std::string::npos);
    const auto restored = RoomFiles::load(path);
    REQUIRE(restored.room);
    REQUIRE(restored.room->scenery[1].animations.at("walk").flipX);
    for (const auto& parameter : roomParameters()) {
        REQUIRE(parameter.read(room) == parameter.read(*restored.room));
    }
    REQUIRE(restored.room->id == room.id);
    REQUIRE(restored.room->width == 2560.0F);
    REQUIRE(restored.room->height == 1440.0F);
    REQUIRE(restored.room->floorColor.b == room.floorColor.b);
    REQUIRE(restored.room->scenery[1].bounds.x == 2100.0F);
    REQUIRE(restored.room->scenery[1].collisionFootprint.h == 21.5F);
    REQUIRE(restored.room->scenery[1].id == "red_box");

    const struct { const char* text; const char* error; } invalidRooms[]{
        {"player.far_speed = nan", "Linha 1: Chave ou valor invalido: player.far_speed"},
        {"typo = 40", "Linha 1: Chave ou valor invalido: typo"},
        {"player.far_speed = 40 px", "Linha 1: Chave ou valor invalido: player.far_speed"},
        {"player.width = 0", "Linha 1: Fora do intervalo: player.width"},
        {"room.width = 1000", "Linha 1: Fora do intervalo: room.width"},
        {"room.height = 600", "Linha 1: Fora do intervalo: room.height"},
        {"player.start_x = 1500", "Posicao inicial do player fora da sala"},
        {"room.vanishing_x = 1500", "vanishing_x fora da largura da sala"},
        {"player.far_speed = 40\nplayer.far_speed = 50", "Linha 2: Chave repetida: player.far_speed"},
        {"# comentario\r\n[room]\r\n\r\nplayer.far_speed 40", "Linha 4: Esperado chave = valor"},
        {"perspective.far_y = 500\nperspective.near_y = 500", "far_y deve ser menor que near_y"},
    };
    for (const auto& invalid : invalidRooms) {
        { std::ofstream bad(directory / "bad.room", std::ios::binary); bad << invalid.text; }
        const auto result = RoomFiles::load(directory / "bad.room");
        REQUIRE((!result.room && result.error == invalid.error));
    }
    const auto validSpeed = restored.room->playerNearSpeed;
    room.nearDepthY = room.farDepthY;
    REQUIRE(!RoomFiles::save(path, room, error));
    REQUIRE(RoomFiles::load(path).room->playerNearSpeed == validSpeed);
}

// D12: every [object] error of Room::validationIssue reaches the designer as "Linha N: ...",
// N being the key that fed it or, when the file has no such key, the block header.
TEST_CASE("testRoomObjectErrorsCarryLines") {
    const TempDirectory testDirectory("pointclick-editor-tests");
    const std::filesystem::path& directory = testDirectory.path();
    const auto errorOf = [&](const std::string& text) {
        {
            std::ofstream bad(directory / "bad_object.room", std::ios::binary);
            bad << text;
        }
        const auto result = RoomFiles::load(directory / "bad_object.room", sharedPlayerArt());
        REQUIRE(!result.room);
        return result.error;
    };
    const std::string box = "# caixa\n[object box]\nbounds = 10, 300, 40, 40\n";
    // Invalid or repeated ID: refused on the header itself.
    REQUIRE(errorOf("[room]\n[object caixa azul]\nbounds = 10, 300, 40, 40\n")
            == "Linha 2: ID de objeto invalido ou repetido");
    REQUIRE(errorOf(box + "\n[object box]\nbounds = 90, 300, 40, 40\n")
            == "Linha 5: ID de objeto invalido ou repetido");
    // Object outside the room: the bounds line, or the header when bounds is missing.
    REQUIRE(errorOf(box + "[object far]\ncolor = 1, 2, 3, 255\nbounds = 1270, 300, 40, 40\n")
            == "Linha 6: Objeto fora da sala: far");
    REQUIRE(errorOf(box + "[object empty]\ncolor = 1, 2, 3, 255\n")
            == "Linha 4: Objeto fora da sala: empty");
    // Initial animation that no clip declares.
    REQUIRE(errorOf(box + "animation.open.file = door.png\nanimation.initial = closed\n")
            == "Linha 5: Animacao inicial ausente: box");
    REQUIRE(errorOf(box + "animation.open.file = door.png\n")
            == "Linha 2: Animacao inicial ausente: box");
    // Collision footprint that does not fit the bounds (or a solid without one).
    REQUIRE(errorOf(box + "collision = 0, 30, 80, 10\n")
            == "Linha 4: Base de colisao invalida: box");
    REQUIRE(errorOf(box + "type = solid\n") == "Linha 2: Base de colisao invalida: box");
    // Errors of the room as a whole keep their text, with no line to point at.
    REQUIRE(errorOf(box + "[room]\nperspective.far_y = 500\nperspective.near_y = 500\n")
            == "far_y deve ser menor que near_y");
    // The same blocks, valid, still load.
    {
        std::ofstream good(directory / "good_object.room", std::ios::binary);
        good << box << "type = solid\ncollision = 0, 30, 40, 10\n";
    }
    REQUIRE(RoomFiles::load(directory / "good_object.room", sharedPlayerArt()).room);
}

// step 1 finds one-pixel lines and text; the default samples large areas quickly.
bool surfaceContainsColor(SDL_Surface* surface, const SDL_Color target, const int step = 8) {
    for (int y = 0; y < surface->h; y += step) {
        for (int x = 0; x < surface->w; x += step) {
            SDL_Color color{};
            REQUIRE(SDL_ReadSurfacePixel(surface, x, y, &color.r, &color.g, &color.b, &color.a));
            if (color.r == target.r && color.g == target.g && color.b == target.b) {
                return true;
            }
        }
    }
    return false;
}

TEST_CASE("testDebugRendering") {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    REQUIRE(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window* window = SDL_CreateWindow("Editor test", 1280, 720, SDL_WINDOW_HIDDEN);
    REQUIRE(window);
    Renderer renderer;
    REQUIRE(renderer.initialize(window));
    Game game(RoomPresets::firstRoom());
    DebugEditor editor;
    const DebugOverlay overlay(editor);
    renderer.addOverlay(&overlay);

    // Debug drawing belongs to the tools: with the editor closed the frame has only the hint;
    // active, it shows the route and the readout, each switched with `show <view> <0|1>`.
    const SDL_Color routeColor{237, 224, 157, 255};
    const SDL_Color crossColor{246, 206, 124, 255};
    const SDL_Color readoutColor{9, 12, 19, 255};
    const SDL_Color hintColor{160, 175, 190, 255};
    const auto frameHas = [&](const SDL_Color color) {
        renderer.render(game, 1.0F);
        SDL_Surface* frame = SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr);
        REQUIRE(frame);
        const bool found = surfaceContainsColor(frame, color, 1);
        SDL_DestroySurface(frame);
        return found;
    };
    game.onClick({300.0F, 600.0F}); // to the left, clear of the editor panel
    game.tick(0.05F);
    REQUIRE(game.scene().player().moving());
    REQUIRE((!frameHas(routeColor) && !frameHas(crossColor) && !frameHas(readoutColor)));
    REQUIRE(frameHas(hintColor));
    editor.onTextInput("DEBUG", game);
    REQUIRE((frameHas(routeColor) && frameHas(crossColor) && frameHas(readoutColor)));
    REQUIRE(!frameHas(hintColor));
    command(editor, game, "show route 0");
    REQUIRE((!editor.views().route && editor.status() == "route: oculto"));
    REQUIRE((!frameHas(routeColor) && !frameHas(crossColor) && frameHas(readoutColor)));
    command(editor, game, "show diagnostics 0");
    REQUIRE(!frameHas(readoutColor));
    command(editor, game, "show grade 1");
    REQUIRE(editor.status() == "Use: show route|diagnostics|hint 0/1");
    command(editor, game, "show route 2");
    REQUIRE((editor.status() == "Use: show route|diagnostics|hint 0/1" && !editor.views().route));
    command(editor, game, "show hint 0");
    REQUIRE((editor.onToolAction(ToolAction::close, game) && !editor.active()));
    REQUIRE((!frameHas(hintColor) && !frameHas(routeColor) && !frameHas(readoutColor)));

    editor.onTextInput("DEBUG", game);
    command(editor, game, "show route 1");
    command(editor, game, "show diagnostics 1");
    command(editor, game, "show hint 1");
    REQUIRE((editor.views().route && editor.views().diagnostics && editor.views().hint));
    renderer.render(game, 1.0F);
    SDL_Surface* pixels = SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr);
    REQUIRE(pixels);
    if (const char* capture = std::getenv("POINTCLICK_DEBUG_CAPTURE"))
        REQUIRE(SDL_SaveBMP(pixels, capture));
    SDL_DestroySurface(pixels);
    // Resizing preserves the logical center used by the mouse and editor.
    REQUIRE(SDL_SetWindowSize(window, 1000, 1000));
    SDL_PumpEvents();
    renderer.render(game, 1.0F);
    SDL_FPoint center{};
    REQUIRE(renderer.windowToWorld(500, 500, center));
    REQUIRE((std::abs(center.x - 640) < 0.1F && std::abs(center.y - 360) < 0.1F));
    SDL_FPoint bar{};
    REQUIRE(renderer.windowToWorld(500, 20, bar));
    REQUIRE(bar.y < 0);
    REQUIRE(!Renderer::containsViewportPoint(bar));

    // Panoramic room: 2K, 4K, and Ultrawide windows must keep the exact 1280x720 camera FOV
    // without revealing off-camera scenery outside [camera.x, camera.x + 1280).
    Room panorama = RoomPresets::firstRoom();
    panorama.width = 2560.0F;
    panorama.height = 720.0F;
    panorama.playerStart = {400.0F, 520.0F};
    const SDL_Color offscreenBeaconColor{24, 244, 92, 255};
    panorama.scenery = {
        {{1500.0F, 360.0F, 220.0F, 220.0F}, offscreenBeaconColor,
         SceneObjectType::decoration, {}, "beacon", {}, "idle"},
    };
    game.loadRoom(panorama);
    REQUIRE(std::abs(game.scene().camera().position().x) < 0.01F);

    const struct { int w; int h; } resolutions[]{
        {1280, 720},
        {1920, 1080},
        {2560, 1440}, // 2K QHD
        {3840, 2160}, // 4K UHD
        {3440, 1440}, // 21:9 Ultrawide (pillarboxed)
    };
    for (const auto res : resolutions) {
        REQUIRE(SDL_SetWindowSize(window, res.w, res.h));
        SDL_PumpEvents();
        renderer.render(game, 1.0F, nullptr);

        SDL_FRect presentation{};
        REQUIRE(SDL_GetRenderLogicalPresentationRect(SDL_GetRenderer(window), &presentation));
        SDL_FPoint vpCenter{};
        SDL_FPoint worldCenter{};
        const float centerX = static_cast<float>(res.w) * 0.5F;
        const float centerY = static_cast<float>(res.h) * 0.5F;
        REQUIRE(renderer.windowToViewport(centerX, centerY, vpCenter));
        REQUIRE(renderer.windowToWorld(centerX, centerY, worldCenter,
                                       game.scene().camera().position()));
        REQUIRE((std::abs(vpCenter.x - 640.0F) < 0.5F && std::abs(vpCenter.y - 360.0F) < 0.5F));
        REQUIRE((std::abs(worldCenter.x - 640.0F) < 0.5F
                 && std::abs(worldCenter.y - 360.0F) < 0.5F));

        SDL_FPoint leftEdge{};
        SDL_FPoint rightEdge{};
        REQUIRE(renderer.windowToWorld(presentation.x + 0.5F, centerY, leftEdge,
                                       game.scene().camera().position()));
        REQUIRE(renderer.windowToWorld(presentation.x + presentation.w - 0.5F, centerY, rightEdge,
                                       game.scene().camera().position()));
        REQUIRE((leftEdge.x >= 0.0F && rightEdge.x <= 1280.0F));
        REQUIRE(std::abs((rightEdge.x - leftEdge.x) - 1280.0F) < 2.0F);

        SDL_Surface* frame = SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr);
        REQUIRE(frame);
        // The beacon at x = 1500..1720 must remain completely invisible while camera.x == 0.
        REQUIRE(!surfaceContainsColor(frame, offscreenBeaconColor));
        SDL_DestroySurface(frame);
    }

    // Once the camera scrolls to center on x = 1550 (camera.x = 910), the beacon enters view
    // while Ultrawide pillarbox bars remain outside the viewport and keep the clear color.
    std::string teleportError;
    REQUIRE(game.teleportPlayer({1550.0F, 620.0F}, teleportError));
    REQUIRE(std::abs(game.scene().camera().position().x - 910.0F) < 0.01F);
    REQUIRE(SDL_SetWindowSize(window, 3440, 1440));
    SDL_PumpEvents();
    renderer.render(game, 1.0F, nullptr);

    SDL_FPoint ultrawideBar{};
    REQUIRE(renderer.windowToViewport(10.0F, 720.0F, ultrawideBar));
    REQUIRE((ultrawideBar.x < 0.0F && !Renderer::containsViewportPoint(ultrawideBar)));

    SDL_FPoint scrolledCenterViewport{};
    SDL_FPoint scrolledCenterWorld{};
    REQUIRE(renderer.windowToViewport(1720.0F, 720.0F, scrolledCenterViewport));
    REQUIRE(renderer.windowToWorld(1720.0F, 720.0F, scrolledCenterWorld,
                                   game.scene().camera().position()));
    REQUIRE(std::abs(scrolledCenterViewport.x - 640.0F) < 0.5F);
    REQUIRE(std::abs(scrolledCenterWorld.x - 1550.0F) < 0.5F);

    SDL_Surface* scrolledFrame = SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr);
    REQUIRE(scrolledFrame);
    REQUIRE((scrolledFrame->w == 2560 && scrolledFrame->h == 1440));
    REQUIRE(surfaceContainsColor(scrolledFrame, offscreenBeaconColor));
    SDL_DestroySurface(scrolledFrame);

    if (SDL_Surface* windowSurface = SDL_GetWindowSurface(window)) {
        REQUIRE((windowSurface->w == 3440 && windowSurface->h == 1440));
        SDL_Color leftBar{};
        SDL_Color rightBar{};
        REQUIRE(SDL_ReadSurfacePixel(windowSurface, 10, 720, &leftBar.r, &leftBar.g, &leftBar.b,
                                     &leftBar.a));
        REQUIRE(SDL_ReadSurfacePixel(windowSurface, 3430, 720, &rightBar.r, &rightBar.g,
                                     &rightBar.b, &rightBar.a));
        REQUIRE((leftBar.r == 8 && leftBar.g == 10 && leftBar.b == 16));
        REQUIRE((rightBar.r == 8 && rightBar.g == 10 && rightBar.b == 16));
    }

    renderer.shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
}

TEST_CASE("testMultiplatformInputAndRouting") {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    REQUIRE(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window* window = SDL_CreateWindow("Input routing test", 3440, 1440, SDL_WINDOW_HIDDEN);
    REQUIRE(window);
    Renderer renderer;
    REQUIRE(renderer.initialize(window));
    REQUIRE(SDL_SetWindowSize(window, 3440, 1440));
    SDL_PumpEvents();

    // Panoramic room (2560x720) with no obstacles so every floor point is walkable.
    Room panorama = RoomPresets::firstRoom();
    panorama.width = 2560.0F;
    panorama.height = 720.0F;
    panorama.scenery.clear();
    panorama.playerStart = {1550.0F, 520.0F}; // Camera snaps to x = 910 (1550 - 640).
    Game game(panorama);
    REQUIRE(std::abs(game.scene().camera().position().x - 910.0F) < 0.01F);
    renderer.render(game, 1.0F, nullptr);

    DebugEditor editor;
    InputSystem input;
    InputRouter router;
    router.setToolConsumer(&editor);
    const ViewportCoordinateMapper mapper = renderer.viewportMapper();

    // 1. DebugEditor panel hit-test: blocks clicks when visible, releases when hidden or closed.
    REQUIRE(!editor.consumesViewportPoint({900.0F, 500.0F}));
    REQUIRE(editor.onTextInput("DEBUG", game));
    REQUIRE((editor.active() && editor.panelVisible()));
    REQUIRE(editor.consumesViewportPoint({900.0F, 500.0F}));
    REQUIRE(!editor.consumesViewportPoint({300.0F, 500.0F}));

    // In 3440x1440 Ultrawide, logical 1280x720 renders in [440..3000] x [0..1440] (scale 2.0).
    // Window (2240, 1000) -> Viewport (900, 500), which lies inside DebugEditor::panelRect().
    SDL_Event panelClick{};
    panelClick.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    panelClick.button.which = 1;
    panelClick.button.button = SDL_BUTTON_LEFT;
    panelClick.button.down = true;
    panelClick.button.x = 440.0F + 900.0F * 2.0F;
    panelClick.button.y = 500.0F * 2.0F;
    auto routeResult = router.handleEvent(panelClick, input, game, mapper);
    REQUIRE(routeResult.consumedByDebug);
    REQUIRE(!game.scene().player().moving());

    // Clicking outside the panel at Viewport (300, 520) -> World (1210, 520) walks the player.
    SDL_Event outsidePanelClick = panelClick;
    outsidePanelClick.button.x = 440.0F + 300.0F * 2.0F;
    outsidePanelClick.button.y = 520.0F * 2.0F;
    routeResult = router.handleEvent(outsidePanelClick, input, game, mapper);
    REQUIRE(!routeResult.consumedByDebug);
    REQUIRE(game.scene().player().moving());
    REQUIRE(std::abs(game.scene().player().destination().x - 1210.0F) < 0.5F);
    REQUIRE(std::abs(game.scene().player().destination().y - 520.0F) < 0.5F);

    std::string teleportError;
    REQUIRE(game.teleportPlayer({1550.0F, 520.0F}, teleportError));

    // Hiding the panel with TAB allows clicking at Viewport (900, 500) -> World (1810, 500).
    SDL_Event tabKey{};
    tabKey.type = SDL_EVENT_KEY_DOWN;
    tabKey.key.key = SDLK_TAB;
    tabKey.key.down = true;
    REQUIRE(router.handleEvent(tabKey, input, game, mapper).consumedByDebug);
    REQUIRE((editor.active() && !editor.panelVisible()));
    REQUIRE(!editor.consumesViewportPoint({900.0F, 500.0F}));

    routeResult = router.handleEvent(panelClick, input, game, mapper);
    REQUIRE(!routeResult.consumedByDebug);
    REQUIRE(game.scene().player().moving());
    REQUIRE(std::abs(game.scene().player().destination().x - 1810.0F) < 0.5F);
    REQUIRE(std::abs(game.scene().player().destination().y - 500.0F) < 0.5F);

    REQUIRE(game.teleportPlayer({1550.0F, 520.0F}, teleportError));

    // Closing the editor with ESC also leaves the panel area unblocked.
    SDL_Event escKey{};
    escKey.type = SDL_EVENT_KEY_DOWN;
    escKey.key.key = SDLK_ESCAPE;
    escKey.key.down = true;
    REQUIRE(router.handleEvent(escKey, input, game, mapper).consumedByDebug);
    REQUIRE(!editor.active());
    routeResult = router.handleEvent(panelClick, input, game, mapper);
    REQUIRE(!routeResult.consumedByDebug);
    REQUIRE(game.scene().player().moving());
    REQUIRE(std::abs(game.scene().player().destination().x - 1810.0F) < 0.5F);

    REQUIRE(game.teleportPlayer({1550.0F, 520.0F}, teleportError));

    // The mouse leaving the window: its last motion was still inside, so only this event tells.
    SDL_Event mouseMotion{};
    mouseMotion.type = SDL_EVENT_MOUSE_MOTION;
    mouseMotion.motion.x = 2240.0F; // viewport (900, 500)
    mouseMotion.motion.y = 1000.0F;
    (void)router.handleEvent(mouseMotion, input, game, mapper);
    REQUIRE(game.scene().pointerInside());
    SDL_Event mouseLeave{};
    mouseLeave.type = SDL_EVENT_WINDOW_MOUSE_LEAVE;
    (void)router.handleEvent(mouseLeave, input, game, mapper);
    REQUIRE(!game.scene().pointerInside());
    REQUIRE(std::abs(input.cursorViewport().x - 900.0F) < 0.5F); // the cursor stays where it left
    REQUIRE(!game.scene().player().moving());
    (void)router.handleEvent(mouseMotion, input, game, mapper);
    REQUIRE(game.scene().pointerInside());

    // 2. Touch translation + letterbox/pillarbox rejection + SDL_TOUCH_MOUSEID filtering.
    // Touch inside left Ultrawide pillarbox bar (normalized x = 0.02 -> window x = 68.8 < 440).
    SDL_Event pillarboxTouch{};
    pillarboxTouch.type = SDL_EVENT_FINGER_DOWN;
    pillarboxTouch.tfinger.touchID = 10;
    pillarboxTouch.tfinger.fingerID = 1;
    pillarboxTouch.tfinger.x = 0.02F;
    pillarboxTouch.tfinger.y = 0.75F;
    (void)router.handleEvent(pillarboxTouch, input, game, mapper);
    REQUIRE(!game.scene().player().moving());
    REQUIRE(!game.scene().pointerInside());

    SDL_Event pillarboxTouchUp = pillarboxTouch;
    pillarboxTouchUp.type = SDL_EVENT_FINGER_UP;
    (void)router.handleEvent(pillarboxTouchUp, input, game, mapper);

    // Touch at window center (normalized 0.5, 0.75) -> Viewport (640, 540) -> World (1550, 540).
    // Let's tap slightly right of center: window x = 440 + 800 * 2 = 2040 -> nx = 2040 / 3440.
    SDL_Event worldTouch{};
    worldTouch.type = SDL_EVENT_FINGER_DOWN;
    worldTouch.tfinger.touchID = 10;
    worldTouch.tfinger.fingerID = 2;
    worldTouch.tfinger.x = (440.0F + 800.0F * 2.0F) / 3440.0F;
    worldTouch.tfinger.y = 540.0F / 720.0F;
    (void)router.handleEvent(worldTouch, input, game, mapper);
    REQUIRE(input.activeDevice() == InputDevice::touch);
    REQUIRE(game.scene().player().moving());
    REQUIRE(std::abs(game.scene().player().destination().x - 1710.0F) < 0.5F); // 800 + 910
    REQUIRE(std::abs(game.scene().player().destination().y - 540.0F) < 0.5F);

    // Synthetic mouse motion & click emitted by touch must be ignored (no double-click / retarget).
    SDL_Event syntheticMouseClick{};
    syntheticMouseClick.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    syntheticMouseClick.button.which = kTouchMouseId;
    syntheticMouseClick.button.button = SDL_BUTTON_LEFT;
    syntheticMouseClick.button.down = true;
    syntheticMouseClick.button.x = 440.0F + 200.0F * 2.0F;
    syntheticMouseClick.button.y = 600.0F * 2.0F;
    (void)router.handleEvent(syntheticMouseClick, input, game, mapper);
    REQUIRE(input.activeDevice() == InputDevice::touch);
    REQUIRE(std::abs(game.scene().player().destination().x - 1710.0F) < 0.5F);

    SDL_Event syntheticMouseTouch{};
    syntheticMouseTouch.type = SDL_EVENT_FINGER_DOWN;
    syntheticMouseTouch.tfinger.touchID = kMouseTouchId;
    syntheticMouseTouch.tfinger.fingerID = 3;
    syntheticMouseTouch.tfinger.x = 0.3F;
    syntheticMouseTouch.tfinger.y = 0.8F;
    (void)router.handleEvent(syntheticMouseTouch, input, game, mapper);
    REQUIRE(std::abs(game.scene().player().destination().x - 1710.0F) < 0.5F);

    SDL_Event worldTouchUp = worldTouch;
    worldTouchUp.type = SDL_EVENT_FINGER_UP;
    (void)router.handleEvent(worldTouchUp, input, game, mapper);

    REQUIRE(game.teleportPlayer({1550.0F, 520.0F}, teleportError));

    // 3. Gamepad hotplug, deadzone, virtual cursor movement, and scrolled world click.
    SDL_Event padAdded{};
    padAdded.type = SDL_EVENT_GAMEPAD_ADDED;
    padAdded.gdevice.which = 77;
    (void)router.handleEvent(padAdded, input, game, mapper);
    REQUIRE(input.gamepadCount() == 1);

    input.setCursorViewport({640.0F, 360.0F});
    input.setCursorSpeed(400.0F);

    // Sub-deadzone stick deflection must produce zero movement.
    SDL_Event smallTilt{};
    smallTilt.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    smallTilt.gaxis.which = 77;
    smallTilt.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTX;
    smallTilt.gaxis.value = 3000; // ~0.09 < 0.24 deadzone
    (void)router.handleEvent(smallTilt, input, game, mapper);
    REQUIRE((input.moveAxis().x == 0.0F && input.moveAxis().y == 0.0F));
    router.update(0.5F, input, game);
    REQUIRE(std::abs(input.cursorViewport().x - 640.0F) < 0.01F);

    // Full stick deflection right & down moves the virtual cursor smoothly in viewport space.
    SDL_Event stickRight = smallTilt;
    stickRight.gaxis.value = 32767;
    (void)router.handleEvent(stickRight, input, game, mapper);
    REQUIRE(std::abs(input.moveAxis().x - 1.0F) < 0.01F);

    SDL_Event stickDown = smallTilt;
    stickDown.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTY;
    stickDown.gaxis.value = 32767;
    (void)router.handleEvent(stickDown, input, game, mapper);
    REQUIRE(std::hypot(input.moveAxis().x, input.moveAxis().y) <= 1.001F);

    // Zero Y, move purely right by 200px (0.5s * 400px/s) and down by 100px: cursor -> (840, 460).
    stickDown.gaxis.value = 0;
    (void)router.handleEvent(stickDown, input, game, mapper);
    router.update(0.5F, input, game);
    REQUIRE(std::abs(input.cursorViewport().x - 840.0F) < 0.1F);

    stickRight.gaxis.value = 0;
    (void)router.handleEvent(stickRight, input, game, mapper);
    stickDown.gaxis.value = 32767;
    (void)router.handleEvent(stickDown, input, game, mapper);
    router.update(0.25F, input, game);
    REQUIRE(std::abs(input.cursorViewport().y - 460.0F) < 0.1F);

    // Primary gamepad button (SOUTH) clicks at virtual cursor (840, 460) -> World (1750, 460).
    SDL_Event southPress{};
    southPress.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    southPress.gbutton.which = 77;
    southPress.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
    southPress.gbutton.down = true;
    (void)router.handleEvent(southPress, input, game, mapper);
    REQUIRE(input.activeDevice() == InputDevice::gamepad);
    REQUIRE(game.scene().player().moving());
    REQUIRE(std::abs(game.scene().player().destination().x - 1750.0F) < 0.5F); // 840 + 910
    REQUIRE(std::abs(game.scene().player().destination().y - 460.0F) < 0.5F);

    // The gamepad's virtual cursor is not the mouse: the OS pointer leaving changes nothing.
    REQUIRE(game.scene().pointerInside());
    (void)router.handleEvent(mouseLeave, input, game, mapper);
    REQUIRE(game.scene().pointerInside());

    // Virtual cursor clamps cleanly at viewport boundaries [0..1279] x [0..719].
    router.update(10.0F, input, game);
    REQUIRE(Renderer::containsViewportPoint(input.cursorViewport()));
    REQUIRE(std::abs(input.cursorViewport().y - 719.0F) < 0.01F);

    // Unplugging the gamepad resets held stick axes to prevent drift.
    SDL_Event padRemoved{};
    padRemoved.type = SDL_EVENT_GAMEPAD_REMOVED;
    padRemoved.gdevice.which = 77;
    (void)router.handleEvent(padRemoved, input, game, mapper);
    REQUIRE(input.gamepadCount() == 0);
    REQUIRE((input.moveAxis().x == 0.0F && input.moveAxis().y == 0.0F));

    // 4. DebugEditor open/navigate/adjust via Keyboard (F1 + arrows) and Gamepad (BACK + D-Pad/Shoulders).
    SDL_Event f1Key{};
    f1Key.type = SDL_EVENT_KEY_DOWN;
    f1Key.key.key = SDLK_F1;
    f1Key.key.down = true;
    REQUIRE(router.handleEvent(f1Key, input, game, mapper).consumedByDebug);
    REQUIRE((editor.active() && editor.panelVisible()));

    SDL_Event padBack{};
    padBack.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    padBack.gbutton.which = 77;
    padBack.gbutton.button = SDL_GAMEPAD_BUTTON_BACK;
    padBack.gbutton.down = true;
    REQUIRE(router.handleEvent(padBack, input, game, mapper).consumedByDebug);
    REQUIRE(!editor.active()); // Toggled closed by gamepad BACK.
    REQUIRE(router.handleEvent(padBack, input, game, mapper).consumedByDebug);
    REQUIRE((editor.active() && editor.panelVisible())); // Re-opened without typing "DEBUG".

    const float initialFarSpeed = game.scene().room().playerFarSpeed;
    REQUIRE(editor.selected() == 0);

    SDL_Event dpadRight = padBack;
    dpadRight.gbutton.button = SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    REQUIRE(router.handleEvent(dpadRight, input, game, mapper).consumedByDebug);
    REQUIRE(game.scene().room().playerFarSpeed > initialFarSpeed);

    SDL_Event leftShoulder = padBack;
    leftShoulder.gbutton.button = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    REQUIRE(router.handleEvent(leftShoulder, input, game, mapper).consumedByDebug);
    REQUIRE(std::abs(game.scene().room().playerFarSpeed - initialFarSpeed) < 0.01F);

    SDL_Event dpadDown = padBack;
    dpadDown.gbutton.button = SDL_GAMEPAD_BUTTON_DPAD_DOWN;
    REQUIRE(router.handleEvent(dpadDown, input, game, mapper).consumedByDebug);
    REQUIRE(editor.selected() == 1);

    SDL_Event keyUp{};
    keyUp.type = SDL_EVENT_KEY_DOWN;
    keyUp.key.key = SDLK_UP;
    keyUp.key.down = true;
    REQUIRE(router.handleEvent(keyUp, input, game, mapper).consumedByDebug);
    REQUIRE(editor.selected() == 0);

    SDL_Event padWest = padBack;
    padWest.gbutton.button = SDL_GAMEPAD_BUTTON_WEST;
    REQUIRE(router.handleEvent(padWest, input, game, mapper).consumedByDebug);
    REQUIRE((editor.active() && !editor.panelVisible()));
    REQUIRE(router.handleEvent(padWest, input, game, mapper).consumedByDebug);
    REQUIRE(editor.panelVisible());

    SDL_Event padEast = padBack;
    padEast.gbutton.button = SDL_GAMEPAD_BUTTON_EAST;
    REQUIRE(router.handleEvent(padEast, input, game, mapper).consumedByDebug);
    REQUIRE(!editor.active());

    // 5. Layer 3 ViewportUiConsumer priority & Layer 1 mobile/console lifecycle signals.
    struct MockModalUi final : ViewportUiConsumer {
        SDL_FRect modalBox{200.0F, 200.0F, 400.0F, 200.0F};
        int navCount = 0;
        bool onPrimaryPress(const SDL_FPoint vp) override {
            return SDL_PointInRectFloat(&vp, &modalBox);
        }
        bool onNavigation(const UiNavAction action) override {
            if (action == UiNavAction::down) ++navCount;
            return true;
        }
    } modalUi;
    router.addUiConsumer(&modalUi);

    REQUIRE(game.teleportPlayer({1550.0F, 520.0F}, teleportError));
    SDL_Event modalClick = panelClick;
    modalClick.button.x = 440.0F + 300.0F * 2.0F; // Viewport (300, 300) inside modalBox
    modalClick.button.y = 300.0F * 2.0F;
    routeResult = router.handleEvent(modalClick, input, game, mapper);
    REQUIRE(routeResult.consumedByUi);
    REQUIRE(!game.scene().player().moving());

    routeResult = router.handleEvent(dpadDown, input, game, mapper);
    REQUIRE((routeResult.consumedByUi && modalUi.navCount == 1));
    router.clearUiConsumers();

    // Lifecycle signals: background clears transient axes, device reset signals invalidation.
    stickRight.gaxis.value = 32767;
    (void)router.handleEvent(stickRight, input, game, mapper);
    REQUIRE(input.moveAxis().x > 0.5F);

    SDL_Event bgEvent{};
    bgEvent.type = SDL_EVENT_WILL_ENTER_BACKGROUND;
    routeResult = router.handleEvent(bgEvent, input, game, mapper);
    REQUIRE(routeResult.enteredBackground);
    REQUIRE((input.moveAxis().x == 0.0F && input.moveAxis().y == 0.0F));

    SDL_Event fgEvent{};
    fgEvent.type = SDL_EVENT_DID_ENTER_FOREGROUND;
    REQUIRE(router.handleEvent(fgEvent, input, game, mapper).enteredForeground);

    SDL_Event resetEvent{};
    resetEvent.type = SDL_EVENT_RENDER_DEVICE_RESET;
    REQUIRE(router.handleEvent(resetEvent, input, game, mapper).invalidateGraphics);
    renderer.invalidateTextures();

    SDL_Event quitEvent{};
    quitEvent.type = SDL_EVENT_QUIT;
    REQUIRE(router.handleEvent(quitEvent, input, game, mapper).quitRequested);

    renderer.shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
}

TEST_CASE("testEditorPreservesSessionState") {
    const TempDirectory testDirectory("pointclick-editor-tests");
    const std::filesystem::path& directory = testDirectory.path();
    Room room = RoomPresets::firstRoom();
    const SDL_Color pickupColor{251, 31, 189, 255};
    room.scenery.push_back({{500.0F, 480.0F, 60.0F, 60.0F}, pickupColor,
                            SceneObjectType::decoration, {}, "magenta_gem", {}, "idle"});

    Game game(room);
    RoomInteractions rules = RoomPresets::firstRoomInteractions();
    rules.rules.push_back({
        .trigger = InteractionTrigger::interact("magenta_gem"),
        .conditions = {InteractionCondition::make("object_visible", {"magenta_gem"})},
        .effects = {
            InteractionEffect::make("give_item", {"magenta_gem"}),
            InteractionEffect::make("hide_object", {"magenta_gem"}),
            InteractionEffect::make("say", {"Peguei a gema magenta!"}),
        },
    });
    game.setInteractions(rules);

    // 1. Collect brass_key from blue_box, unlock solid red_box via useItemOnObject, and pick up magenta_gem.
    game.onClick({430.0F, 410.0F});
    for (int i = 0; i < 80; ++i) game.tick(0.05F);
    REQUIRE(game.session().hasFlag("found_brass_key"));
    REQUIRE(game.session().hasItem("brass_key"));

    REQUIRE(game.useItemOnObject("brass_key", "red_box"));
    for (int i = 0; i < 100; ++i) game.tick(0.05F);
    REQUIRE(!game.session().hasItem("brass_key"));
    REQUIRE(game.session().hasFlag("red_box_unlocked"));
    REQUIRE(!game.scene().isObjectSolid(game.scene().room().scenery[1]));
    REQUIRE(game.scene().navigation().canStandAt({860.0F, 575.0F}));

    game.onClick({530.0F, 510.0F});
    for (int i = 0; i < 80; ++i) game.tick(0.05F);
    REQUIRE(game.session().hasItem("magenta_gem"));
    REQUIRE(!game.scene().isObjectVisible("magenta_gem"));

    // 2. Live edits in DebugEditor (set, move, undo, reset) preserve SessionState.
    DebugEditor editor;
    const RoomLibrary library = openLibrary(directory);
    editor.setLibrary(library);
    REQUIRE(editor.onTextInput("DEBUG", game));
    REQUIRE(editor.active());

    command(editor, game, "set player.far_speed 180");
    REQUIRE(game.scene().room().playerFarSpeed == 180.0F);
    REQUIRE(game.session().hasItem("magenta_gem"));
    REQUIRE(game.session().hasFlag("found_brass_key"));
    REQUIRE(game.session().hasFlag("red_box_unlocked"));
    REQUIRE(!game.scene().isObjectVisible("magenta_gem"));
    REQUIRE(!game.scene().isObjectSolid(game.scene().room().scenery[1]));
    REQUIRE(game.scene().navigation().canStandAt({860.0F, 575.0F}));

    command(editor, game, "move blue_box 300 350");
    REQUIRE(game.scene().room().scenery[0].bounds.x == 300.0F);
    REQUIRE(game.session().hasItem("magenta_gem"));
    REQUIRE(!game.scene().isObjectVisible("magenta_gem"));
    REQUIRE(game.scene().navigation().canStandAt({860.0F, 575.0F}));

    command(editor, game, "undo");
    REQUIRE(game.scene().room().scenery[0].bounds.x == 380.0F);
    REQUIRE(game.session().hasItem("magenta_gem"));
    REQUIRE(game.session().hasFlag("red_box_unlocked"));
    REQUIRE(!game.scene().isObjectVisible("magenta_gem"));
    REQUIRE(game.scene().navigation().canStandAt({860.0F, 575.0F}));

    command(editor, game, "reset");
    REQUIRE(game.scene().room().playerFarSpeed == 110.0F);
    REQUIRE(game.session().hasItem("magenta_gem"));
    REQUIRE(game.session().hasFlag("red_box_unlocked"));
    REQUIRE(!game.scene().isObjectVisible("magenta_gem"));
    REQUIRE(game.scene().navigation().canStandAt({860.0F, 575.0F}));
    editor.onToolAction(ToolAction::close, game);

    // 3. Headless SceneRenderer check: hidden magenta_gem is not drawn; showing it draws it again.
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    REQUIRE(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window* window = SDL_CreateWindow("Visibility render test", 1280, 720, SDL_WINDOW_HIDDEN);
    REQUIRE(window);
    Renderer renderer;
    REQUIRE(renderer.initialize(window));

    renderer.render(game, 1.0F, nullptr);
    SDL_Surface* hiddenFrame = SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr);
    REQUIRE(hiddenFrame);
    REQUIRE(!surfaceContainsColor(hiddenFrame, pickupColor));
    SDL_DestroySurface(hiddenFrame);

    game.changeSession([&](SessionState& session) {
        session.setObjectVisible(game.scene().room().id, "magenta_gem", true);
    });
    renderer.render(game, 1.0F, nullptr);
    SDL_Surface* shownFrame = SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr);
    REQUIRE(shownFrame);
    REQUIRE(surfaceContainsColor(shownFrame, pickupColor));
    SDL_DestroySurface(shownFrame);

    renderer.shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
}

TEST_CASE("testInteractionRulesFilesAndValidation") {
    const TempDirectory testDirectory("pointclick-editor-tests");
    const std::filesystem::path& directory = testDirectory.path();
    // 1. Validate shipped companion .rules files against shipped .room files.
    const auto roomsDir = std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets/rooms";
    const auto firstRoom = RoomFiles::load(roomsDir / "first.room", sharedPlayerArt());
    REQUIRE(firstRoom.room);
    const auto firstRules = InteractionFiles::loadOptionalForRoom(
            roomsDir / "first.room", &*firstRoom.room);
    REQUIRE((firstRules.interactions && firstRules.error.empty()));
    REQUIRE(!firstRules.interactions->empty());
    REQUIRE(firstRules.interactions->hasRulesForObject("blue_box"));
    REQUIRE(firstRules.interactions->hasRulesForObject("red_box"));

    const auto galleryRoom = RoomFiles::load(roomsDir / "gallery.room", sharedPlayerArt());
    REQUIRE(galleryRoom.room);
    const auto galleryRules = InteractionFiles::loadOptionalForRoom(
            roomsDir / "gallery.room", &*galleryRoom.room);
    REQUIRE((galleryRules.interactions && galleryRules.error.empty()));
    REQUIRE(galleryRules.interactions->hasRulesForObject("plant"));
    REQUIRE(galleryRules.interactions->hasRulesForObject("table"));

    // 2. Optional companion loading succeeds with empty rules when no .rules exists.
    const auto lonelyRoomPath = directory / "no_rules_here.room";
    std::string saveError;
    REQUIRE(RoomFiles::save(lonelyRoomPath, *firstRoom.room, saveError));
    const auto optionalMissing = InteractionFiles::loadOptionalForRoom(
            lonelyRoomPath, &*firstRoom.room);
    REQUIRE((optionalMissing.interactions && optionalMissing.interactions->empty()));
    REQUIRE(optionalMissing.error.empty());

    // Direct load on a missing .rules file returns a clear open error.
    const auto directMissing = InteractionFiles::load(directory / "missing.rules");
    REQUIRE(!directMissing.interactions);
    REQUIRE(directMissing.error == "Nao foi possivel abrir missing.rules");

    // 3. Full save/load roundtrip covering all triggers, conditions, effects, quotes, and '#' in strings.
    Room testRoom = RoomPresets::firstRoom();
    testRoom.scenery[1].animations["idle"] = SpriteClip{.file = "red_idle.png"};
    testRoom.scenery[1].animations["open"] = SpriteClip{.file = "red_open.png"};

    const std::string authoredRulesText =
        "# Comentario de topo\r\n"
        "[interact blue_box] # inspecao\r\n"
        "flag_not_set opened_once\r\n"
        "lacks_item golden_token\r\n"
        "object_visible blue_box\r\n"
        "set_flag opened_once\r\n"
        "give_item golden_token\r\n"
        "say \"Item #1: \\\"ficha dourada\\\" encontrada!\" # comentario no fim\r\n"
        "\r\n"
        "[use_item golden_token red_box]\r\n"
        "flag_set opened_once\r\n"
        "has_item golden_token\r\n"
        "object_visible red_box\r\n"
        "take_item golden_token\r\n"
        "clear_flag opened_once\r\n"
        "hide_object blue_box\r\n"
        "show_object red_box\r\n"
        "set_solid red_box 0\r\n"
        "play_animation red_box open\r\n"
        "change_room gallery\r\n"
        "say \"Passagem liberada!\"\r\n"
        "\r\n"
        "[interact red_box]\r\n"
        "object_hidden blue_box\r\n"
        "say \"Ja passamos por aqui.\"\r\n";

    const auto parsed = InteractionFiles::parse(authoredRulesText, &testRoom);
    REQUIRE((parsed.interactions && parsed.error.empty()));
    REQUIRE(parsed.interactions->rules.size() == 3);
    REQUIRE(parsed.interactions->rules[0].effects[2].args[0]
            == "Item #1: \"ficha dourada\" encontrada!");

    const auto rulesPath = directory / "roundtrip.rules";
    REQUIRE(InteractionFiles::save(rulesPath, *parsed.interactions, saveError, &testRoom));
    const auto reloaded = InteractionFiles::load(rulesPath, &testRoom);
    REQUIRE((reloaded.interactions && reloaded.error.empty()));
    REQUIRE(reloaded.interactions->rules.size() == 3);
    REQUIRE(reloaded.interactions->rules[0].effects[2].args[0]
            == "Item #1: \"ficha dourada\" encontrada!");
    REQUIRE(reloaded.interactions->rules[1].trigger.kind == InteractionTriggerKind::useItem);
    REQUIRE(reloaded.interactions->rules[1].trigger.itemId == "golden_token");
    REQUIRE(reloaded.interactions->rules[1].trigger.objectId == "red_box");
    REQUIRE(reloaded.interactions->rules[1].effects.size() == 8);

    // F4 / C12: wildcard triggers parse against the room (no object lookup for `*`) and
    // survive save -> load unchanged.
    {
        const auto wild = InteractionFiles::parse(
                "[use_item * red_box]\nsay \"a\"\n[use_item golden_token *]\nsay \"b\"\n"
                "[use_item * *]\nsay \"c\"\n[interact *]\nsay \"d\"\n",
                &testRoom);
        REQUIRE((wild.interactions && wild.error.empty()));
        const auto& rules = wild.interactions->rules;
        REQUIRE((rules[0].trigger.itemId == "*" && rules[0].trigger.objectId == "red_box"));
        REQUIRE((rules[1].trigger.itemId == "golden_token" && rules[1].trigger.objectId == "*"));
        REQUIRE((rules[3].trigger.kind == InteractionTriggerKind::interact
                 && rules[3].trigger.objectId == "*"));
        REQUIRE((rules[0].trigger.wildcardLevel() == 1 && rules[1].trigger.wildcardLevel() == 2
                 && rules[2].trigger.wildcardLevel() == 3
                 && rules[3].trigger.wildcardLevel() == 1));
        const auto wildPath = directory / "wildcard.rules";
        REQUIRE(InteractionFiles::save(wildPath, *wild.interactions, saveError, &testRoom));
        const std::string written = *TextFormat::readFile(wildPath).text;
        REQUIRE(written.find("[use_item * red_box]") != std::string::npos);
        REQUIRE(written.find("[use_item golden_token *]") != std::string::npos);
        REQUIRE(written.find("[use_item * *]") != std::string::npos);
        REQUIRE(written.find("[interact *]") != std::string::npos);
        const auto wildBack = InteractionFiles::load(wildPath, &testRoom);
        REQUIRE((wildBack.interactions && wildBack.interactions->rules.size() == 4));
        for (std::size_t i = 0; i < 4; ++i) {
            REQUIRE(wildBack.interactions->rules[i].trigger.objectId == rules[i].trigger.objectId);
            REQUIRE(wildBack.interactions->rules[i].trigger.itemId == rules[i].trigger.itemId);
        }
        // `*` is only a whole token: anything else is still a bad identifier.
        REQUIRE(InteractionFiles::parse("[interact **]\nsay \"x\"\n", &testRoom).error
                == "Linha 1: Identificador de objeto invalido: **");
        REQUIRE(InteractionFiles::parse("[use_item a* red_box]\nsay \"x\"\n", &testRoom).error
                == "Linha 1: Identificador de item invalido: a*");
    }

    // 4. DebugEditor 'load' rejects a room if its companion .rules file has a syntax error,
    // preserving the active simulation.
    const auto brokenCompanionRoom = directory / "broken_rules.room";
    Room brokenRulesRoom = testRoom;
    brokenRulesRoom.id = "broken_rules"; // the library requires id == file name
    REQUIRE(RoomFiles::save(brokenCompanionRoom, brokenRulesRoom, saveError));
    {
        std::ofstream badCompanion(directory / "broken_rules.rules", std::ios::binary);
        badCompanion << "[interact blue_box]\nsay \"sem fechar aspas\n";
    }
    Game editorGame(testRoom);
    DebugEditor editor;
    const RoomLibrary library = openLibrary(directory);
    editor.setLibrary(library);
    REQUIRE(editor.onTextInput("DEBUG", editorGame));
    command(editor, editorGame, "set player.far_speed 155");
    REQUIRE(editorGame.scene().room().playerFarSpeed == 155.0F);
    command(editor, editorGame, "load broken_rules.room");
    REQUIRE(editor.status() == "Linha 2: Texto entre aspas nao fechado");
    REQUIRE(editorGame.scene().room().playerFarSpeed == 155.0F); // Rolled back.
    command(editor, editorGame, "reload"); // no reloader wired (as in these tests): reported
    REQUIRE(editor.status() == "Recarga de sprites indisponivel");

    // 5. Clear line-by-line error messages for invalid rule syntax and invalid room references.
    const struct {
        const char* text;
        const char* expectedError;
    } invalidCases[]{
        {"say \"fora de bloco\"",
         "Linha 1: Regra deve comecar com [interact <objeto>] ou [use_item <item> <objeto>]"},
        {"[interact blue_box",
         "Linha 1: Cabecalho de gatilho malformado"},
        {"[touch blue_box]\nsay \"oi\"",
         "Linha 1: Gatilho desconhecido: touch"},
        {"[interact]\nsay \"oi\"",
         "Linha 1: Use [interact <objeto>]"},
        {"[use_item key]\nsay \"oi\"",
         "Linha 1: Use [use_item <item> <objeto>]"},
        {"[interact blue_box]\nflag_set ok\n",
         "Linha 1: Regra sem efeitos"},
        {"[interact blue_box]\nsay \"oi\"\nflag_set tarde_demais",
         "Linha 3: Condicao deve vir antes dos efeitos"},
        {"[interact blue_box]\nsay sem_aspas",
         "Linha 2: Use say \"texto\""},
        {"[interact blue_box]\nsay \"\"",
         "Linha 2: Texto vazio em say"},
        {"[interact blue_box]\nsay \"ok\" lixo",
         "Linha 2: Caracteres extras apos aspas em say"},
        {"[interact blue_box]\nset_solid red_box 2",
         "Linha 2: Use set_solid <objeto> <0|1>"},
        {"[interact blue_box]\ncomando_inexistente 123",
         "Linha 2: Comando de regra desconhecido: comando_inexistente"},
        {"[interact blue_box]\nset_flag flag$invalida",
         "Linha 2: Identificador invalido: flag$invalida"},
        {"[interact fantasma]\nsay \"oi\"",
         "Linha 1: Objeto desconhecido na sala: fantasma"},
        {"[interact blue_box]\nhide_object fantasma",
         "Linha 2: Objeto desconhecido na sala: fantasma"},
        {"[interact blue_box]\nplay_animation red_box danca",
         "Linha 2: Animacao desconhecida 'danca' no objeto: red_box"},
    };

    for (const auto& testCase : invalidCases) {
        const auto bad = InteractionFiles::parse(testCase.text, &testRoom);
        REQUIRE(!bad.interactions);
        REQUIRE(bad.error == testCase.expectedError);
    }
}

void assertSameSteps(const std::vector<InteractionEffect>& a, const std::vector<InteractionEffect>& b) {
    REQUIRE(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        REQUIRE((a[i].effect == b[i].effect && a[i].args == b[i].args));
    }
}

void assertSameDialogues(const RoomDialogues& a, const RoomDialogues& b) {
    REQUIRE(a.nodes.size() == b.nodes.size());
    for (std::size_t n = 0; n < a.nodes.size(); ++n) {
        const DialogueNode& x = a.nodes[n];
        const DialogueNode& y = b.nodes[n];
        REQUIRE((x.id == y.id && x.choices.size() == y.choices.size()));
        assertSameSteps(x.lines, y.lines);
        for (std::size_t c = 0; c < x.choices.size(); ++c) {
            const DialogueChoice& p = x.choices[c];
            const DialogueChoice& q = y.choices[c];
            REQUIRE((p.id == q.id && p.text == q.text && p.once == q.once && p.next == q.next));
            REQUIRE(p.conditions.size() == q.conditions.size());
            for (std::size_t i = 0; i < p.conditions.size(); ++i) {
                REQUIRE(p.conditions[i].condition == q.conditions[i].condition);
                REQUIRE(p.conditions[i].args == q.conditions[i].args);
            }
            assertSameSteps(p.effects, q.effects);
        }
    }
}

TEST_CASE("testDialogueFiles") {
    const TempDirectory testDirectory("pointclick-editor-tests");
    const std::filesystem::path& directory = testDirectory.path();
    // 1. The shipped gallery.dlg loads against gallery.room and matches gallery.rules.
    const auto roomsDir = std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets/rooms";
    const auto gallery = RoomFiles::load(roomsDir / "gallery.room", sharedPlayerArt());
    REQUIRE(gallery.room);
    const auto shipped = DialogueFiles::loadOptionalForRoom(roomsDir / "gallery.room",
                                                            &*gallery.room);
    REQUIRE((shipped.dialogues && shipped.error.empty()));
    const DialogueNode* table = shipped.dialogues->findNode("mesa");
    REQUIRE((table && table->choices.size() == 3));
    REQUIRE(table->findChoice("ler_bilhete")->conditions.size() == 1);
    REQUIRE((table->findChoice("gavetas")->once
             && table->findChoice("gavetas")->next == "fundo_falso"));
    const auto galleryRules = InteractionFiles::loadOptionalForRoom(roomsDir / "gallery.room",
                                                                    &*gallery.room);
    REQUIRE(galleryRules.interactions);
    REQUIRE(dialogueReferencesError(*galleryRules.interactions, *shipped.dialogues).empty());
    const auto firstRoom = RoomFiles::load(roomsDir / "first.room", sharedPlayerArt());
    const auto firstNone = DialogueFiles::loadOptionalForRoom(roomsDir / "first.room",
                                                              &*firstRoom.room);
    REQUIRE((firstNone.dialogues && firstNone.dialogues->empty() && firstNone.error.empty()));
    REQUIRE(!DialogueFiles::load(directory / "missing.dlg").dialogues);

    // 2. Round trip: parse -> save -> load keeps every field, and saving is stable.
    Room room = RoomPresets::firstRoom();
    room.scenery[1].animations["open"] = SpriteClip{.file = "red_open.png"};
    const std::string authored = R"(# comentario
[node guarda]
say "Alto la! #1"   # comentario apos a fala
play_animation red_box open

[choice chave "Tenho a \"chave\"."]
has_item brass_key
flag_not_set guard_open
once
take_item brass_key
set_flag guard_open
goto liberado

[choice tchau "Tchau."]

[node liberado]
say "Pode passar."
wait 0.5
)";
    const auto parsed = DialogueFiles::parse(authored, &room);
    REQUIRE((parsed.dialogues && parsed.error.empty()));
    const DialogueChoice* key = parsed.dialogues->findNode("guarda")->findChoice("chave");
    REQUIRE((key->text == "Tenho a \"chave\"." && key->once && key->next == "liberado"));
    REQUIRE((key->conditions.size() == 2 && key->effects.size() == 2));
    REQUIRE(parsed.dialogues->findNode("guarda")->lines[0].args[0] == "Alto la! #1");
    REQUIRE(parsed.dialogues->findNode("guarda")->findChoice("tchau")->effects.empty());

    const auto path = directory / "roundtrip.dlg";
    std::string error;
    REQUIRE(DialogueFiles::save(path, *parsed.dialogues, error, &room));
    const auto reloaded = DialogueFiles::load(path, &room);
    REQUIRE((reloaded.dialogues && reloaded.error.empty()));
    assertSameDialogues(*parsed.dialogues, *reloaded.dialogues);
    const auto path2 = directory / "roundtrip2.dlg";
    REQUIRE(DialogueFiles::save(path2, *reloaded.dialogues, error, &room));
    std::ifstream first(path), second(path2);
    const std::string firstText((std::istreambuf_iterator<char>(first)), {});
    const std::string secondText((std::istreambuf_iterator<char>(second)), {});
    REQUIRE((!firstText.empty() && firstText == secondText));
    REQUIRE(!std::filesystem::exists(directory / "roundtrip.dlg.tmp"));

    // Invalid definitions are refused before touching the file.
    RoomDialogues broken = *parsed.dialogues;
    broken.nodes[0].choices[0].next = "nowhere";
    REQUIRE(!DialogueFiles::save(path, broken, error, &room));
    REQUIRE(error == "Destino de goto desconhecido: nowhere");
    REQUIRE(DialogueFiles::load(path, &room).dialogues);

    // 3. Line-by-line validation names the line and the expected form.
    const struct {
        const char* text;
        const char* expectedError;
    } invalidCases[] = {
        {"say \"oi\"\n", "Linha 1: Dialogo deve comecar com [node <id>]"},
        {"[node a b]\nsay \"x\"\n", "Linha 1: Use [node <id>]"},
        {"[node a]\n", "Linha 1: No de dialogo vazio: a"},
        {"[node a]\n[node b]\nsay \"x\"\n", "Linha 1: No de dialogo vazio: a"},
        {"[node a]\nsay \"x\"\n[node a]\nsay \"y\"\n", "Linha 3: No de dialogo repetido: a"},
        {"[choice c \"x\"]\n", "Linha 1: Escolha deve vir depois de um [node <id>]"},
        {"[node a]\n[choice c]\n", "Linha 2: Use [choice <id> \"texto\"]"},
        {"[node a]\n[choice c \"\"]\n", "Linha 2: Escolha sem texto: c"},
        {"[node a]\n[choice c \"x\"]\n[choice c \"y\"]\n", "Linha 3: Escolha repetida no no a: c"},
        {"[node a]\n[menu]\n", "Linha 2: Bloco desconhecido: menu (use [node <id>] ou [choice <id> \"texto\"])"},
        {"[node a]\nhas_item k\n", "Linha 2: has_item so vale dentro de [choice <id> \"texto\"]"},
        {"[node a]\ngoto a\n", "Linha 2: goto so vale dentro de [choice <id> \"texto\"]"},
        {"[node a]\n[choice c \"x\"]\nsay \"y\"\nhas_item k\n", "Linha 4: Condicao deve vir antes dos efeitos"},
        {"[node a]\n[choice c \"x\"]\ngoto a\nsay \"y\"\n", "Linha 4: goto deve ser a ultima linha da escolha"},
        {"[node a]\n[choice c \"x\"]\ngoto\n", "Linha 3: Use goto <no>"},
        {"[node a]\n[choice c \"x\"]\ngoto b\n", "Linha 3: Destino de goto desconhecido: b"},
        {"[node a]\n[choice c \"x\"]\nonce\nonce\n", "Linha 4: once repetido na escolha"},
        {"[node a]\n[choice c \"x\"]\nonce agora\n", "Linha 3: Use once"},
        {"[node a]\nstart_dialogue a\n", "Linha 2: start_dialogue nao vale dentro de dialogos; use goto <no>"},
        {"[node a]\nhide_object fantasma\n", "Linha 2: Objeto desconhecido na sala: fantasma"},
        {"[node a]\ndance\n", "Linha 2: Comando de dialogo desconhecido: dance"},
        {"[node a]\n[choice c \"x\"]\nset_solid red_box 2\n", "Linha 3: Use set_solid <objeto> <0|1>"},
    };
    for (const auto& testCase : invalidCases) {
        const auto bad = DialogueFiles::parse(testCase.text, &room);
        REQUIRE(!bad.dialogues);
        if (bad.error != testCase.expectedError) {
            SDL_Log("dialogue case: got '%s' expected '%s'", bad.error.c_str(), testCase.expectedError);
        }
        REQUIRE(bad.error == testCase.expectedError);
    }
}

// Choice menu: modal only while a choice is expected; click/touch, up/down + confirm, gamepad South.
// The world drops free text, so typing during a cutscene never replaces the line.
TEST_CASE("testTypedTextNeverBecomesSpeech") {
    Game game(RoomPresets::firstRoom());
    game.setInteractions(RoomInteractions{{{
        .trigger = InteractionTrigger::interact("blue_box"),
        .conditions = {},
        // A say that is not the last step holds the queue: this is the cutscene being protected.
        .effects = {InteractionEffect::make("say", {"Linha"}),
                    InteractionEffect::make("set_flag", {"line_done"})},
    }}});
    game.onClick({430.0F, 410.0F});
    for (int i = 0; i < 200 && !game.inCutscene(); ++i) game.tick(0.05F);
    REQUIRE((game.inCutscene() && game.scene().floatingText().text() == "Linha"));

    TranslatedInput typed;
    typed.textInput = "x";
    const InputRouter storeRouter; // no tools layer, as in a store build
    const InputRouteResult storeResult = storeRouter.routeTranslated(typed, game);
    REQUIRE((!storeResult.consumedByDebug && !storeResult.consumedByUi));
    REQUIRE((game.inCutscene() && game.scene().floatingText().text() == "Linha"));

    DebugEditor editor;
    InputRouter toolsRouter;
    toolsRouter.setToolConsumer(&editor);
    typed.textInput = "DEBU"; // not yet the activation word: the editor lets it through
    REQUIRE(!toolsRouter.routeTranslated(typed, game).consumedByDebug);
    REQUIRE(!editor.active());
    REQUIRE((game.inCutscene() && game.scene().floatingText().text() == "Linha"));

    // The line still ends after its reading time (2 s for "Linha"), not a 10 s echo.
    for (int i = 0; i < 50 && game.inCutscene(); ++i) game.tick(0.05F);
    REQUIRE((!game.inCutscene() && game.session().hasFlag("line_done")));
}

TEST_CASE("testDialogueMenuWrapsLongOptions") {
    Game game(RoomPresets::firstRoom());
    const std::string longText(80, 'x'); // 99 glyphs fit a row: the word moves to line 2
    DialogueChoice a{.id = "a", .text = "Curta", .conditions = {}, .effects = {}};
    DialogueChoice b{.id = "b", .text = "Você tem certeza de que " + longText, .conditions = {},
                     .effects = {}};
    game.setDialogues(RoomDialogues{{DialogueNode{"menu", {}, {a, b}}}});
    constexpr MonospaceTextMetrics metrics{12.0F, 28.0F};
    DialogueMenu menu(game, metrics);
    REQUIRE((game.startDialogue("menu") && game.awaitingDialogueChoice()));
    REQUIRE(menu.rowCount() == 2);
    const DialogueMenu::Row& wrappedRow = menu.row(1);
    REQUIRE(wrappedRow.lines.size() == 2);
    REQUIRE(wrappedRow.rect.h == 2.0F * 28.0F + DialogueMenu::kRowPaddingY * 2.0F);
    for (const TextLine& line : wrappedRow.lines) {
        REQUIRE(line.width <= wrappedRow.rect.w - DialogueMenu::kTextInsetX * 2.0F);
    }
    // The taller row moves the panel up; it still ends at the bottom margin.
    const SDL_FRect panel = menu.panelRect();
    REQUIRE(panel.y + panel.h == 720.0F - DialogueMenu::kMargin);
    REQUIRE(panel.h
            == DialogueMenu::kPadding * 2.0F + DialogueMenu::kMinRowHeight + wrappedRow.rect.h);
    REQUIRE(menu.optionAt({640.0F, wrappedRow.rect.y + wrappedRow.rect.h - 2.0F}) == 1u);
}

TEST_CASE("testDialogueMenuInput") {
    Game game(RoomPresets::firstRoom());
    DialogueChoice a{.id = "a", .text = "Primeira", .conditions = {},
                     .effects = {InteractionEffect::make("set_flag", {"picked_a"})}};
    DialogueChoice b{.id = "b", .text = "Segunda", .conditions = {},
                     .effects = {InteractionEffect::make("set_flag", {"picked_b"})}};
    DialogueChoice c{.id = "c", .text = "Terceira", .conditions = {},
                     .effects = {InteractionEffect::make("set_flag", {"picked_c"})}};
    game.setDialogues(RoomDialogues{{DialogueNode{
            "menu", {InteractionEffect::make("say", {"Escolha"})}, {a, b, c}}}});
    constexpr MonospaceTextMetrics metrics{12.0F, 28.0F};
    DialogueMenu menu(game, metrics);
    InputRouter router;
    router.addUiConsumer(&menu);
    DebugEditor editor;
    router.setToolConsumer(&editor);

    const auto press = [&](const SDL_FPoint point) {
        TranslatedInput input;
        input.pointerMoved = true;
        input.primaryPressed = true;
        input.viewportPoint = point;
        input.insideViewport = true;
        return router.routeTranslated(input, game);
    };
    const auto navigate = [&](const UiNavAction action) {
        TranslatedInput input;
        input.navAction = action;
        return router.routeTranslated(input, game);
    };
    const auto center = [](const SDL_FRect rect) {
        return SDL_FPoint{rect.x + rect.w * 0.5F, rect.y + rect.h * 0.5F};
    };

    // Closed: presses reach the world.
    REQUIRE(!press({900.0F, 600.0F}).consumedByUi);
    REQUIRE(game.scene().player().moving());
    for (int i = 0; i < 200 && game.scene().player().moving(); ++i) game.tick(0.05F);
    REQUIRE(!navigate(UiNavAction::down).consumedByUi);

    // Closed: no rows, nothing to hit.
    REQUIRE((menu.rowCount() == 0 && !menu.optionAt({640.0F, 650.0F})));

    // Click/touch on a row chooses it.
    REQUIRE(game.startDialogue("menu"));
    REQUIRE(game.awaitingDialogueChoice());

    // Layout: rows stack inside the panel, at the bottom of the 1280x720 viewport, sized by the
    // measured text ("2. Segunda" is one 28-unit line, so the 40-unit minimum row applies).
    REQUIRE((menu.rowCount() == 3 && menu.row(1).label == "2. Segunda"));
    const SDL_FRect panel = menu.panelRect();
    REQUIRE((panel.y + panel.h <= 720.0F && panel.x >= 0.0F && panel.x + panel.w <= 1280.0F));
    const SDL_FRect row1 = menu.optionRect(1);
    REQUIRE((row1.h == DialogueMenu::kMinRowHeight && row1.y == menu.optionRect(0).y + row1.h));
    REQUIRE(menu.optionAt(center(row1)) == 1u);
    REQUIRE(!menu.optionAt({10.0F, 10.0F}));
    const SDL_FPoint text = menu.lineOrigin(1, 0);
    REQUIRE((text.x == row1.x + DialogueMenu::kTextInsetX && text.y == row1.y + 6.0F));
    const SDL_FPoint feet = game.scene().player().feet();
    REQUIRE(press({200.0F, 100.0F}).consumedByUi); // modal: outside the rows does nothing
    REQUIRE((game.awaitingDialogueChoice() && !game.scene().player().moving()));
    REQUIRE(game.scene().player().feet().x == feet.x);
    REQUIRE(press(center(row1)).consumedByUi);
    REQUIRE((game.session().hasFlag("picked_b") && !game.dialogueActive()));

    // Keyboard / D-pad: down wraps, confirm picks the highlighted row.
    REQUIRE(game.startDialogue("menu"));
    REQUIRE(menu.highlighted() == 0);
    REQUIRE(navigate(UiNavAction::up).consumedByUi);
    REQUIRE(menu.highlighted() == 2);
    REQUIRE(navigate(UiNavAction::down).consumedByUi);
    REQUIRE(menu.highlighted() == 0);
    REQUIRE((navigate(UiNavAction::cancel).consumedByUi && game.awaitingDialogueChoice()));
    REQUIRE(navigate(UiNavAction::confirm).consumedByUi);
    REQUIRE(game.session().hasFlag("picked_a"));

    // Gamepad: D-pad highlights, South (a press at the unmoved virtual cursor) confirms it.
    REQUIRE(game.startDialogue("menu"));
    TranslatedInput hover;
    hover.pointerMoved = true;
    hover.viewportPoint = {200.0F, 100.0F};
    hover.insideViewport = true;
    (void)router.routeTranslated(hover, game);
    REQUIRE(navigate(UiNavAction::down).consumedByUi);
    REQUIRE(navigate(UiNavAction::down).consumedByUi);
    REQUIRE(press({200.0F, 100.0F}).consumedByUi);
    REQUIRE(game.session().hasFlag("picked_c"));

    // Hover follows the pointer and is consumed inside the panel.
    REQUIRE(game.startDialogue("menu"));
    hover.viewportPoint = center(menu.optionRect(2));
    REQUIRE(router.routeTranslated(hover, game).consumedByUi);
    REQUIRE((menu.highlighted() == 2 && !game.scene().pointerInside()));

    // The editor active with its panel hidden (TAB, to test the room) leaves arrows and Enter to
    // the menu; the same keys with the panel showing stay with the editor.
    const auto key = [&](const ToolAction tool, const UiNavAction nav) {
        TranslatedInput input;
        input.toolAction = tool;
        input.navAction = nav;
        return router.routeTranslated(input, game);
    };
    REQUIRE(editor.onToolAction(ToolAction::toggleEditor, game));
    REQUIRE(editor.panelVisible());
    REQUIRE(key(ToolAction::previous, UiNavAction::up).consumedByDebug);
    REQUIRE(menu.highlighted() == 2);
    REQUIRE(editor.onToolAction(ToolAction::togglePanel, game));
    REQUIRE((editor.active() && !editor.panelVisible()));
    const InputRouteResult hiddenUp = key(ToolAction::previous, UiNavAction::up);
    REQUIRE((hiddenUp.consumedByUi && !hiddenUp.consumedByDebug));
    REQUIRE(menu.highlighted() == 1);
    REQUIRE(key(ToolAction::submit, UiNavAction::confirm).consumedByUi);
    REQUIRE(!game.dialogueActive());
    REQUIRE((editor.onToolAction(ToolAction::close, game) && !editor.active()));
    REQUIRE(game.startDialogue("menu"));

    // Draws without a window of its own (software renderer) while open.
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    REQUIRE(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window* window = SDL_CreateWindow("Dialogue test", 1280, 720, SDL_WINDOW_HIDDEN);
    REQUIRE(window);
    Renderer renderer;
    REQUIRE(renderer.initialize(window));
    renderer.render(game, 1.0F, nullptr, &menu);
    SDL_Surface* pixels = SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr);
    REQUIRE(pixels);
    if (const char* capture = std::getenv("POINTCLICK_DIALOGUE_CAPTURE")) {
        REQUIRE(SDL_SaveBMP(pixels, capture));
    }
    SDL_DestroySurface(pixels);
    renderer.shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
}

// Every catalog entry, present and future, must survive parse -> save -> load with its typed
// arguments, so a new effect or condition is covered without writing a new test.
std::string sampleArgument(const ArgType type) {
    switch (type) {
    case ArgType::objectId: return "red_box";
    case ArgType::animation: return "open";
    case ArgType::text: return "Linha \"um\" #1\nfim";
    case ArgType::boolean: return "1";
    case ArgType::number: return "1.5";
    case ArgType::integer: return "-3";
    case ArgType::facing: return "left";
    case ArgType::itemId:
    case ArgType::flagId:
    case ArgType::roomId:
    case ArgType::dialogueId:
    case ArgType::spawnId: return "sample_id";
    }
    return {};
}

template <typename Descriptor>
std::vector<std::string> sampleArguments(const Descriptor& descriptor) {
    std::vector<std::string> args;
    for (const ArgType type : descriptor.args.view()) args.push_back(sampleArgument(type));
    return args;
}

TEST_CASE("testActionCatalogRoundtrip") {
    const TempDirectory testDirectory("pointclick-editor-tests");
    const std::filesystem::path& directory = testDirectory.path();
    Room room = RoomPresets::firstRoom();
    room.scenery[1].animations["open"] = SpriteClip{.file = "red_open.png"};

    std::set<std::string_view> keywords;
    InteractionRule rule{.trigger = InteractionTrigger::interact("blue_box")};
    for (const ConditionDescriptor& condition : conditionCatalog()) {
        REQUIRE((!condition.description.empty() && condition.evaluate));
        REQUIRE(keywords.insert(condition.keyword).second);
        rule.conditions.push_back(
                InteractionCondition::make(condition.keyword, sampleArguments(condition)));
    }
    for (const EffectDescriptor& effect : effectCatalog()) {
        REQUIRE((!effect.description.empty() && effect.start));
        REQUIRE(keywords.insert(effect.keyword).second);
        rule.effects.push_back(InteractionEffect::make(effect.keyword, sampleArguments(effect)));
    }
    RoomInteractions authored{{rule}};
    REQUIRE(authored.validationError(&room).empty());

    const auto path = directory / "catalog.rules";
    std::string error;
    REQUIRE(InteractionFiles::save(path, authored, error, &room));
    const auto loaded = InteractionFiles::load(path, &room);
    REQUIRE((loaded.interactions && loaded.error.empty()));
    const InteractionRule& back = loaded.interactions->rules.at(0);
    REQUIRE(back.conditions.size() == rule.conditions.size());
    REQUIRE(back.effects.size() == rule.effects.size());
    for (std::size_t i = 0; i < rule.conditions.size(); ++i) {
        REQUIRE(back.conditions[i].condition == rule.conditions[i].condition);
        REQUIRE(back.conditions[i].args == rule.conditions[i].args);
    }
    for (std::size_t i = 0; i < rule.effects.size(); ++i) {
        REQUIRE(back.effects[i].effect == rule.effects[i].effect);
        REQUIRE(back.effects[i].args == rule.effects[i].args);
    }

    // Typed validation applies to rules built in C++ as well as to parsed files.
    bool threw = false;
    try {
        (void)InteractionEffect::make("teleport", {"x"});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    REQUIRE(threw);
    const auto invalid = [&](InteractionEffect effect) {
        RoomInteractions rules{{{.trigger = InteractionTrigger::interact("blue_box"),
                                 .effects = {std::move(effect)}}}};
        return rules.validationError(&room);
    };
    REQUIRE(invalid(InteractionEffect::make("set_solid", {"red_box", "2"}))
            == "Use set_solid <objeto> <0|1>");
    REQUIRE(invalid(InteractionEffect::make("set_solid", {"red_box"}))
            == "Use set_solid <objeto> <0|1>");
    REQUIRE(invalid(InteractionEffect::make("hide_object", {"fantasma"}))
            == "Objeto desconhecido na sala: fantasma");
    REQUIRE(invalid(InteractionEffect::make("play_animation", {"red_box", "danca"}))
            == "Animacao desconhecida 'danca' no objeto: red_box");
    REQUIRE(invalid(InteractionEffect::make("say", {""})) == "Texto vazio em say");
    REQUIRE(invalid(InteractionEffect::make("wait", {"1,5"})) == "Numero invalido: 1,5");
    REQUIRE(invalid(InteractionEffect::make("wait", {"-1"})) == "Numero invalido: -1");
    REQUIRE(invalid(InteractionEffect::make("add_flag", {"coins", "1.5"}))
            == "Inteiro invalido: 1.5");
    REQUIRE(invalid(InteractionEffect::make("add_flag", {"coins", "+2"}))
            == "Inteiro invalido: +2");
    REQUIRE(invalid(InteractionEffect::make("add_flag", {"coins"})) == "Use add_flag <flag> <n>");
    int counter = 0;
    REQUIRE((parseIntegerArgument("-12", counter) && counter == -12));
    REQUIRE((parseIntegerArgument("1000000", counter) && counter == 1'000'000));
    REQUIRE((!parseIntegerArgument("1000001", counter) && !parseIntegerArgument("-", counter)));
    {
        const auto parsed = InteractionFiles::parse(
                "[interact blue_box]\nflag_at_least coins x\nsay \"oi\"\n", &room);
        REQUIRE((!parsed.interactions && parsed.error == "Linha 2: Inteiro invalido: x"));
        const auto direction = InteractionFiles::parse(
                "[interact blue_box]\nsay \"oi\"\nface norte\n", &room);
        REQUIRE((!direction.interactions
                 && direction.error
                            == "Linha 3: Direcao invalida: norte (use left, right, up ou down)"));
        REQUIRE(invalid(InteractionEffect::make("face", {"idle"}))
                == "Direcao invalida: idle (use left, right, up ou down)");
    }
    float seconds = 0.0F;
    REQUIRE((parseNumberArgument("0.25", seconds) && seconds == 0.25F));
    REQUIRE((parseNumberArgument("12", seconds) && seconds == 12.0F));
    REQUIRE((!parseNumberArgument("1.2.3", seconds) && !parseNumberArgument(".", seconds)));
    REQUIRE(actionUsage("play_animation", findEffect("play_animation")->args)
            == "Use play_animation <objeto> <animacao>");
}
// The shipped layout: rooms inherit assets/actors/player.actor; RoomLibrary loads a room with
// its .rules and .dlg in one validated bundle for both the game and the editor.
TEST_CASE("testRoomLibraryAndActors") {
    const TempDirectory testDirectory("pointclick-editor-tests");
    const std::filesystem::path& directory = testDirectory.path();
    RoomLibrary library;
    std::string error;
    REQUIRE(library.open(std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets/rooms", error));
    REQUIRE(library.playerArt().size() == 5);
    const auto first = library.loadBundle("first.room");
    REQUIRE((first.bundle && first.error.empty()));
    REQUIRE(first.bundle->room.playerAnimations == library.playerArt()); // no copy in the room
    REQUIRE(!first.bundle->interactions.empty());
    const auto gallery = library.loadBundle("gallery.room");
    REQUIRE((gallery.bundle && gallery.bundle->dialogues.findNode("mesa")));
    REQUIRE(gallery.bundle->room.playerAnimations == library.playerArt());

    // Saving through the library keeps the shared art out of the room file.
    RoomLibrary scratch = openLibrary(directory);
    REQUIRE(scratch.playerArt().empty()); // no actors/ folder next to a temp directory
    REQUIRE(RoomFiles::save(directory / "art.room", first.bundle->room, error,
                            &library.playerArt()));
    std::ifstream saved(directory / "art.room");
    const std::string text((std::istreambuf_iterator<char>(saved)), {});
    REQUIRE(text.find("player.idle.") == std::string::npos);

    // The file name is the room's identity (change_room and SessionState must agree on it).
    Room declared = first.bundle->room;
    declared.id = "outra";
    REQUIRE(RoomFiles::save(directory / "trocada.room", declared, error));
    const auto mismatched = scratch.loadBundle("trocada.room");
    REQUIRE(!mismatched.bundle);
    REQUIRE(mismatched.error
            == "id = outra nao corresponde ao arquivo trocada.room (use id = trocada)");
    REQUIRE(!scratch.loadRoom("trocada.room").room);
    // A file without `id` is the room named after it, not Room's default "first".
    std::ifstream declaredFile(directory / "trocada.room");
    std::string withoutId;
    for (std::string line; std::getline(declaredFile, line);) {
        if (!line.starts_with("id = ")) withoutId += line + '\n';
    }
    declaredFile.close();
    std::ofstream(directory / "semid.room") << withoutId;
    const auto unnamed = scratch.loadBundle("semid.room");
    REQUIRE((unnamed.bundle && unnamed.bundle->room.id == "semid"));
    // Saving under another name writes that name's id, so the copy loads back as its own room.
    REQUIRE(scratch.saveRoom("copia.room", declared, error));
    const auto copy = scratch.loadBundle("copia.room");
    REQUIRE((copy.bundle && copy.bundle->room.id == "copia"));

    // Files saved by Windows editors start with a UTF-8 BOM and use CRLF: every format reads
    // them through the same TextFormat reader.
    const std::string bom = "\xEF\xBB\xBF";
    std::ofstream(directory / "bom.room", std::ios::binary) << bom << withoutId;
    std::ofstream(directory / "bom.rules", std::ios::binary)
            << bom << "[interact blue_box]\r\nsay \"oi\"\r\nstart_dialogue caixa\r\n";
    std::ofstream(directory / "bom.dlg", std::ios::binary) << bom << "[node caixa]\r\nsay \"ola\"\r\n";
    std::ofstream(directory / "bom.actor", std::ios::binary) << bom << "idle.file = a.png\r\n";
    const auto withBom = scratch.loadBundle("bom.room");
    REQUIRE((withBom.bundle && withBom.error.empty()));
    REQUIRE(withBom.bundle->room.id == "bom");
    REQUIRE(withBom.bundle->interactions.rules.size() == 1);
    REQUIRE(withBom.bundle->dialogues.findNode("caixa"));
    REQUIRE(ActorFiles::load(directory / "bom.actor").animations);

    REQUIRE(ActorFiles::parse("").error == "Ator sem animacoes");
    REQUIRE(ActorFiles::parse("idle.file = a.png\nidle.file = b.png").error
            == "Linha 2: Chave repetida: idle.file");
    REQUIRE(ActorFiles::parse("idle.frames = 0").error
            == "Animacao idle: Contagem de quadros invalida");
    REQUIRE(ActorFiles::parse("idle.colour = 1").error
            == "Linha 1: Chave ou valor invalido: idle.colour");

    // The shared actor is optional and is found by opening it (assets inside an APK are not on
    // the filesystem): absent = no art and no error; present but empty or broken = an error.
    const auto pack = directory / "pack";
    std::filesystem::create_directories(pack / "rooms");
    std::filesystem::create_directories(pack / "actors");
    const ActorLoadResult absent = ActorFiles::loadOptional(pack / "actors/player.actor");
    REQUIRE((absent.animations && absent.animations->empty() && absent.error.empty()));
    RoomLibrary packed;
    REQUIRE((packed.open(pack / "rooms", error) && packed.playerArt().empty()));
    std::ofstream(pack / "actors/player.actor", std::ios::binary) << "";
    REQUIRE(!packed.open(pack / "rooms", error));
    REQUIRE(error == "player.actor: Ator sem animacoes");
    std::ofstream(pack / "actors/player.actor", std::ios::binary) << "idle.fps = rapido\n";
    REQUIRE(!packed.open(pack / "rooms", error));
    REQUIRE(error == "player.actor: Linha 1: Chave ou valor invalido: idle.fps");
    std::ofstream(pack / "actors/player.actor", std::ios::binary) << "idle.file = a.png\n";
    REQUIRE((packed.open(pack / "rooms", error) && packed.playerArt().size() == 1));
}

// [spawn <id>] in .room (feet + facing) round-trips; change_room takes an optional spawn; the
// content check finds rooms and spawns that a change_room names but that do not exist.
TEST_CASE("testRoomSpawnFilesAndTransitions") {
    const TempDirectory testDirectory("pointclick-editor-tests");
    const std::filesystem::path& directory = testDirectory.path();
    const auto rooms = directory / "spawn" / "rooms";
    std::filesystem::create_directories(rooms);
    std::filesystem::create_directories(directory / "spawn" / "actors");
    std::filesystem::copy_file(std::filesystem::path(POINTCLICK_SOURCE_DIR)
                               / "assets/actors/player.actor",
                               directory / "spawn" / "actors" / "player.actor");
    const auto write = [&](const std::string& name, const std::string& text) {
        std::ofstream(rooms / name, std::ios::binary) << text;
    };
    write("hall.room", "[room]\nid = hall\n"
                       "[spawn door_left] # porta da esquerda\nfeet = 120, 520\nfacing = right\n"
                       "[spawn door_right]\nfeet = 1150.5, 450\n");
    RoomLibrary library = openLibrary(rooms);
    const RoomLoadResult hall = RoomFiles::load(rooms / "hall.room", sharedPlayerArt());
    REQUIRE((hall.room && hall.error.empty()));
    REQUIRE(hall.room->spawns.size() == 2);
    REQUIRE(hall.room->spawns[0] == (RoomSpawn{"door_left", {120.0F, 520.0F}, Facing::right}));
    REQUIRE(hall.room->spawns[1] == (RoomSpawn{"door_right", {1150.5F, 450.0F}, Facing::down}));

    std::string error;
    REQUIRE(RoomFiles::save(rooms / "copy.room", *hall.room, error, sharedPlayerArt()));
    std::ifstream savedFile(rooms / "copy.room");
    const std::string saved((std::istreambuf_iterator<char>(savedFile)), {});
    REQUIRE(saved.find("[spawn door_left]\nfeet = 120, 520\nfacing = right\n")
            != std::string::npos);
    const RoomLoadResult copy = RoomFiles::load(rooms / "copy.room", sharedPlayerArt());
    REQUIRE((copy.room && copy.room->spawns == hall.room->spawns));

    // Saved numbers are the shortest text that reads back the same: no "53.3333321" noise.
    Room fractions = *hall.room;
    fractions.spawns[1].feet = {1150.1F, 450.25F};
    fractions.scenery.push_back({{100.1F, 400.0F, 53.333333F, 60.0F}, {1, 2, 3, 255},
                                 SceneObjectType::decoration, {}, "crate",
                                 {{"idle", SpriteClip{.file = "crate.png", .fps = 7.5F,
                                                      .displayWidth = 53.333333F,
                                                      .offsetY = -0.1F}}},
                                 "idle"});
    REQUIRE(RoomFiles::save(rooms / "fractions.room", fractions, error, sharedPlayerArt()));
    std::ifstream fractionsFile(rooms / "fractions.room");
    const std::string fractionsText((std::istreambuf_iterator<char>(fractionsFile)), {});
    REQUIRE(fractionsText.find("[spawn door_right]\nfeet = 1150.1, 450.25\n") != std::string::npos);
    REQUIRE(fractionsText.find("bounds = 100.1, 400, 53.333332, 60\n") != std::string::npos);
    REQUIRE(fractionsText.find("animation.idle.fps = 7.5\n") != std::string::npos);
    REQUIRE(fractionsText.find("animation.idle.display_width = 53.333332\n") != std::string::npos);
    REQUIRE(fractionsText.find("animation.idle.offset_y = -0.1\n") != std::string::npos);
    REQUIRE(fractionsText.find("room.width = 1280\n") != std::string::npos);
    const RoomLoadResult reread = RoomFiles::load(rooms / "fractions.room", sharedPlayerArt());
    REQUIRE((reread.room && reread.room->spawns == fractions.spawns));
    const SceneObject& crate = reread.room->scenery.back();
    REQUIRE(SDL_RectsEqualEpsilon(&crate.bounds, &fractions.scenery.back().bounds, 0.0F));
    REQUIRE(crate.animations == fractions.scenery.back().animations);
    // Saving what was read gives the same bytes.
    REQUIRE(RoomFiles::save(rooms / "fractions2.room", *reread.room, error, sharedPlayerArt()));
    std::ifstream fractionsAgain(rooms / "fractions2.room");
    REQUIRE(std::string((std::istreambuf_iterator<char>(fractionsAgain)), {}) == fractionsText);

    const auto errorOf = [&](const std::string& text) {
        write("bad.room", text);
        const RoomLoadResult result = RoomFiles::load(rooms / "bad.room", sharedPlayerArt());
        REQUIRE(!result.room);
        return result.error;
    };
    REQUIRE(errorOf("[spawn a]\nfacing = up\n") == "Linha 1: Spawn sem feet: a");
    REQUIRE(errorOf("[spawn a]\nfeet = 1, 2\nfacing = norte\n")
            == "Linha 3: Chave ou valor invalido: facing");
    REQUIRE(errorOf("[spawn a]\nfeet = 1\n") == "Linha 2: Chave ou valor invalido: feet");
    REQUIRE(errorOf("[spawn a]\nfeet = 1, 2\nbounds = 1, 2, 3, 4\n")
            == "Linha 3: Chave ou valor invalido: bounds");
    REQUIRE(errorOf("[spawn a]\nfeet = 1, 2\nfeet = 3, 4\n") == "Linha 3: Chave repetida: feet");
    // D12: what Room::validationIssue finds comes with the line of the block (or key) at fault.
    REQUIRE(errorOf("[spawn a]\nfeet = 1, 2\n[spawn a]\nfeet = 3, 4\n")
            == "Linha 3: ID de spawn invalido ou repetido");
    REQUIRE(errorOf("[room]\n[spawn a b]\nfeet = 1, 2\n")
            == "Linha 2: ID de spawn invalido ou repetido");
    REQUIRE(errorOf("[spawn a]\nfacing = up\nfeet = 1300, 2\n")
            == "Linha 3: Spawn fora da sala: a");
    REQUIRE(errorOf("[spawn ok]\nfeet = 1, 2\n\n[spawn b]\nfacing = up\n")
            == "Linha 4: Spawn sem feet: b");
    // A [spawn] in the middle does not swallow the room keys after a [room] header.
    write("mixed.room", "[spawn a]\nfeet = 1, 400\n[room]\nplayer.far_speed = 99\n");
    const RoomLoadResult mixed = RoomFiles::load(rooms / "mixed.room", sharedPlayerArt());
    REQUIRE((mixed.room && mixed.room->playerFarSpeed == 99.0F));

    // change_room <sala> [<spawn>]: one or two arguments, never three.
    const auto rulesOf = [](const std::string_view statement) {
        return InteractionFiles::parse("[interact x]\n" + std::string(statement) + "\n", nullptr);
    };
    const auto plain = rulesOf("change_room hall");
    REQUIRE((plain.interactions && plain.interactions->rules[0].effects[0].args.size() == 1));
    const auto spawned = rulesOf("change_room hall door_left # porta");
    REQUIRE(spawned.interactions);
    REQUIRE(spawned.interactions->rules[0].effects[0].args
            == (std::vector<std::string>{"hall", "door_left"}));
    REQUIRE(rulesOf("change_room hall door_left extra").error
            == "Linha 2: Use change_room <sala> [<spawn>]");
    REQUIRE(rulesOf("change_room").error == "Linha 2: Use change_room <sala> [<spawn>]");
    REQUIRE(rulesOf("change_room hall porta!").error == "Linha 2: Identificador invalido: porta!");
    const auto path = rooms / "spawned.rules";
    REQUIRE(InteractionFiles::save(path, *spawned.interactions, error, nullptr));
    std::ifstream rulesFile(path);
    const std::string rulesText((std::istreambuf_iterator<char>(rulesFile)), {});
    REQUIRE(rulesText.find("change_room hall door_left\n") != std::string::npos);

    // Content check over a bundle's transitions (rules and dialogue choices).
    RoomBundle bundle{*hall.room, *spawned.interactions, {}};
    REQUIRE(library.transitionsError(bundle).empty());
    bundle.interactions.rules[0].effects[0].args[1] = "window";
    REQUIRE(library.transitionsError(bundle) == "change_room hall: spawn desconhecido window");
    bundle.interactions = *plain.interactions;
    bundle.interactions.rules[0].effects[0].args[0] = "nowhere";
    REQUIRE(library.transitionsError(bundle).starts_with("change_room nowhere: "));
    bundle.interactions.rules.clear();
    DialogueChoice leave{.id = "sair", .text = "Sair"};
    leave.effects = {InteractionEffect::make("change_room", {"hall", "porta"})};
    bundle.dialogues.nodes = {DialogueNode{.id = "guarda", .lines = {}, .choices = {leave}}};
    REQUIRE(library.transitionsError(bundle) == "change_room hall: spawn desconhecido porta");

    // DEBUG `load <sala>.room [spawn]` enters through the spawn; an unknown one changes nothing.
    Game game(RoomPresets::firstRoom());
    DebugEditor editor;
    editor.setLibrary(library);
    REQUIRE((editor.onTextInput("DEBUG", game) && editor.active()));
    command(editor, game, "load hall.room door_left");
    REQUIRE(editor.status() == "Sala carregada: hall.room em door_left");
    REQUIRE((game.scene().room().id == "hall" && game.scene().player().facing() == Facing::right));
    command(editor, game, "load first.room porta!");
    REQUIRE(editor.status() == "Use: load nome.room [spawn] (sem pastas)");
    command(editor, game, "load hall.room window");
    REQUIRE(editor.status() == "Spawn desconhecido na sala hall: window");
    REQUIRE((game.scene().room().id == "hall" && game.scene().player().facing() == Facing::right));
    command(editor, game, "save hall2.room extra");
    REQUIRE(editor.status() == "Use: save nome.room (sem pastas)");

    // The shipped rooms: every change_room points at a room and spawn that exist.
    RoomLibrary assets = openLibrary(std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets/rooms");
    for (const char* name : {"first.room", "gallery.room"}) {
        const RoomBundleResult shipped = assets.loadBundle(name);
        REQUIRE((shipped.bundle && assets.transitionsError(*shipped.bundle).empty()));
    }
}

// ---- F2: CommandCatalog and the session commands (DEBUG panel and --exec share them) ----

std::filesystem::path shippedRooms() {
    return std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets/rooms";
}

// A new game in the shipped first.room (read only) with a console over the shipped library,
// settled the way the headless runner does it. `printed` collects the runner's report.
struct ShippedGame {
    RoomLibrary library = openLibrary(shippedRooms());
    std::unique_ptr<Game> game;
    CommandConsole console;
    std::vector<std::string> printed;
    std::unique_ptr<CommandScript> script;

    ShippedGame() {
        RoomBundleResult loaded = library.loadBundle("first.room");
        REQUIRE(loaded.bundle);
        game = std::make_unique<Game>(std::move(loaded.bundle->room));
        std::string error;
        REQUIRE(game->setInteractions(std::move(loaded.bundle->interactions), error));
        REQUIRE(game->setDialogues(std::move(loaded.bundle->dialogues), error));
        console.setLibrary(library);
        console.begin(*game);
        script = std::make_unique<CommandScript>(
                *game, console, library,
                [this](const std::string_view line) { printed.emplace_back(line); });
        REQUIRE(script->settle());
        printed.clear();
    }
    CommandResult execute(const std::string_view line) { return console.execute(line, *game); }
    bool printedLine(const std::string_view line) const {
        return std::find(printed.begin(), printed.end(), line) != printed.end();
    }
};

bool pendingRoomChange(const Game& game, const std::string_view roomId,
                       const std::string_view spawnId) {
    for (const GameEvent& event : game.pendingEvents()) {
        const auto* change = std::get_if<RoomChangeRequested>(&event);
        if (change && change->roomId == roomId && change->spawnId == spawnId) return true;
    }
    return false;
}

bool hasRoomChange(const Game& game) {
    return std::any_of(game.pendingEvents().begin(), game.pendingEvents().end(),
                       [](const GameEvent& event) {
                           return std::holds_alternative<RoomChangeRequested>(event);
                       });
}

// Every command has a name, a usage starting with it and a description, all in `help` and in
// the panel's list; `help <comando>` answers with the same line.
TEST_CASE("testCommandCatalogHelp") {
    Game game(RoomPresets::firstRoom());
    CommandConsole console;
    console.begin(game);
    const CommandResult help = console.execute("help", game);
    REQUIRE(help.ok);
    REQUIRE(help.output.starts_with("Comandos: " + commandNames() + "\n"));
    const auto printable = [](const std::string_view text) {
        return std::all_of(text.begin(), text.end(),
                           [](const char c) { return c >= 32 && c <= 126; });
    };
    std::set<std::string_view> names;
    for (const CommandDescriptor& command : commandCatalog()) {
        REQUIRE(names.insert(command.name).second);
        REQUIRE(validIdentifier(command.name));
        REQUIRE(command.run != nullptr);
        REQUIRE(findCommand(command.name) == &command);
        REQUIRE((command.usage == command.name
                 || command.usage.starts_with(std::string(command.name) + " ")));
        REQUIRE((!command.description.empty() && printable(command.description)));
        REQUIRE(printable(command.usage)); // the panel draws with the ASCII debug font
        const std::string line = std::string(command.usage) + " -- "
                                 + std::string(command.description);
        REQUIRE(help.output.find("\n" + line) != std::string::npos);
        const std::string listed = " " + commandNames() + " ";
        REQUIRE(listed.find(" " + std::string(command.name) + " ") != std::string::npos);
        const CommandResult one = console.execute("help " + std::string(command.name), game);
        REQUIRE((one.ok && one.output == line));
    }
    // The old commands and the F2 ones, under these names (`show` stays the debug views).
    for (const std::string_view name :
         {"set",     "teleport", "move", "size",    "base",  "solid", "show", "save",
          "load",    "reload",   "undo", "reset",   "flag",  "give",  "take", "showobj",
          "hideobj", "use",      "goto", "newgame", "state", "help"}) {
        REQUIRE(names.contains(name));
    }
    REQUIRE(names.size() == 22);

    const CommandResult unknownTopic = console.execute("help nope", game);
    REQUIRE((!unknownTopic.ok
             && unknownTopic.output == "Comando desconhecido: nope. Digite help."));
    const CommandResult unknown = console.execute("nope 1", game);
    REQUIRE((!unknown.ok && unknown.output == "Comando desconhecido. Digite help."));
    REQUIRE(console.execute("help a b", game).output == "Use: help [comando]");
    const CommandResult blank = console.execute("   ", game);
    REQUIRE((blank.ok && blank.output.empty()));

    // The panel shows the same answer; a blank line keeps the previous status.
    DebugEditor editor;
    REQUIRE(editor.onTextInput("DEBUG", game));
    command(editor, game, "help give");
    REQUIRE(editor.status() == "give <item> -- Poe o item no inventario");
    command(editor, game, "   ");
    REQUIRE(editor.status() == "give <item> -- Poe o item no inventario");
    command(editor, game, "give brass_key");
    REQUIRE((editor.status() == "Item recebido: brass_key" && game.session().hasItem("brass_key")));
}

TEST_CASE("testFlagCommand") {
    Game game(RoomPresets::firstRoom());
    CommandConsole console;
    const auto run = [&](const std::string_view line) { return console.execute(line, game); };
    CommandResult result = run("flag door 3");
    REQUIRE((result.ok && result.output == "Flag door = 3"
             && game.session().flagValue("door") == 3));
    // C13: the command accepts exactly the stored range and refuses beyond it, unchanged.
    REQUIRE(run("flag door -1000000000").ok);
    REQUIRE(game.session().flagValue("door") == -kMaxFlagValue);
    REQUIRE(run("flag door 1000000000").ok);
    REQUIRE(game.session().flagValue("door") == kMaxFlagValue);
    for (const char* line :
         {"flag door 1000000001", "flag door -1000000001", "flag door 99999999999999999999"}) {
        result = run(line);
        REQUIRE(!result.ok);
        REQUIRE(result.output.starts_with("Valor fora do limite de +-1000000000: "));
        REQUIRE(game.session().flagValue("door") == kMaxFlagValue);
    }
    REQUIRE(run("flag door 1000000001").output
            == "Valor fora do limite de +-1000000000: 1000000001");
    for (const char* line :
         {"flag door", "flag door 2x", "flag door 1.5", "flag door 1 2", "flag"}) {
        result = run(line);
        REQUIRE((!result.ok && result.output == "Use: flag <id> <n>"));
    }
    result = run("flag porta! 1");
    REQUIRE((!result.ok && result.output == "Flag invalida: porta!"));
    result = run("flag door 0");
    REQUIRE((result.ok && result.output == "Flag apagada: door"
             && !game.session().hasFlag("door")));
}

TEST_CASE("testItemCommands") {
    Game game(RoomPresets::firstRoom());
    CommandConsole console;
    const auto run = [&](const std::string_view line) { return console.execute(line, game); };
    REQUIRE(run("give brass_key").output == "Item recebido: brass_key");
    REQUIRE(run("give gem").ok);
    REQUIRE(game.session().items() == std::vector<std::string>{"brass_key", "gem"});
    CommandResult result = run("give gem");
    REQUIRE((!result.ok && result.output == "Item ja esta no inventario: gem"));
    result = run("take brass_key");
    REQUIRE((result.ok && result.output == "Item removido: brass_key"));
    REQUIRE(game.session().items() == std::vector<std::string>{"gem"});
    result = run("take brass_key");
    REQUIRE((!result.ok && result.output == "Item fora do inventario: brass_key"));
    REQUIRE(run("give chave!").output == "Item invalido: chave!");
    REQUIRE(run("take").output == "Use: take <item>");
    REQUIRE(run("give a b").output == "Use: give <item>");
    REQUIRE(game.session().items() == std::vector<std::string>{"gem"});
}

TEST_CASE("testObjectVisibilityCommands") {
    const RoomLibrary library = openLibrary(shippedRooms());
    Game game(RoomPresets::firstRoom());
    CommandConsole console;
    console.setLibrary(library);
    const auto run = [&](const std::string_view line) { return console.execute(line, game); };
    REQUIRE(!game.scene().navigation().canStandAt({860.0F, 575.0F})); // red_box's base

    // An object of the room in play: hidden, it stops blocking the floor (changeSession).
    CommandResult result = run("hideobj red_box");
    REQUIRE((result.ok && result.output == "first.red_box: oculto"));
    REQUIRE(!game.scene().isObjectVisible("red_box"));
    REQUIRE(game.scene().navigation().canStandAt({860.0F, 575.0F}));
    result = run("showobj first.red_box");
    REQUIRE((result.ok && result.output == "first.red_box: visivel"));
    REQUIRE(game.scene().isObjectVisible("red_box"));
    REQUIRE(!game.scene().navigation().canStandAt({860.0F, 575.0F}));

    // Another room, checked against its file.
    result = run("hideobj gallery.plant");
    REQUIRE((result.ok && !game.session().isObjectVisible("gallery", "plant")));
    REQUIRE(run("hideobj gallery.nope").output == "Objeto desconhecido: gallery.nope");
    REQUIRE(run("hideobj nope").output == "Objeto desconhecido: first.nope");
    result = run("showobj nowhere.lamp");
    REQUIRE((!result.ok && result.output.starts_with("Sala desconhecida: nowhere (")));
    REQUIRE(run("hideobj a.b.c").output == "Objeto invalido: a.b.c");
    REQUIRE(run("hideobj .red_box").output == "Objeto invalido: .red_box");
    REQUIRE(run("hideobj").output == "Use: hideobj <sala.objeto|objeto>");
    REQUIRE(run("showobj a b").output == "Use: showobj <sala.objeto|objeto>");
    // `show` is still the debug views: an object name there is a usage error.
    REQUIRE(run("show red_box 0").output == "Use: show route|diagnostics|hint 0/1");

    CommandConsole withoutLibrary;
    REQUIRE(withoutLibrary.execute("hideobj gallery.plant", game).output
            == "Pasta de salas nao configurada");
    REQUIRE(withoutLibrary.execute("hideobj blue_box", game).ok); // the room in play needs none
}

TEST_CASE("testUseCommand") {
    Game game(RoomPresets::firstRoom());
    game.setInteractions(RoomPresets::firstRoomInteractions());
    CommandConsole console;
    const auto run = [&](const std::string_view line) { return console.execute(line, game); };
    const auto settle = [&] {
        for (int i = 0; i < 2000 && (game.inCutscene() || game.scene().player().moving()); ++i) {
            game.tick(0.05F);
        }
    };
    CommandResult result = run("use brass_key red_box");
    REQUIRE((!result.ok && result.output == "Item fora do inventario: brass_key (give brass_key)"));
    REQUIRE(run("give brass_key").ok);
    REQUIRE(run("use brass_key nope").output == "Objeto desconhecido: nope");
    REQUIRE(run("use chave! red_box").output == "Item invalido: chave!");
    REQUIRE(run("use brass_key").output == "Use: use <item> <objeto>");
    REQUIRE(run("hideobj blue_box").ok);
    REQUIRE(run("use brass_key blue_box").output == "Objeto oculto: blue_box");
    REQUIRE(run("showobj blue_box").ok);

    // The [use_item brass_key red_box] rule: the player walks there and the rule runs.
    result = run("use brass_key red_box");
    REQUIRE((result.ok && result.output == "Usando brass_key em red_box"));
    REQUIRE(game.scene().player().moving());
    settle();
    REQUIRE((game.session().hasFlag("red_box_unlocked") && !game.session().hasItem("brass_key")));
    REQUIRE(!game.scene().isObjectSolid(*game.scene().room().findObject("red_box")));

    // While a sequence holds the scene the domain refuses item use; the command says why.
    RoomInteractions rules = RoomPresets::firstRoomInteractions();
    rules.rules.push_back({.trigger = InteractionTrigger::useItem("gem", "blue_box"),
                           .conditions = {},
                           .effects = {InteractionEffect::make("say", {"Um."}),
                                       InteractionEffect::make("say", {"Dois."})}});
    game.setInteractions(rules);
    REQUIRE((run("give gem").ok && run("give brass_key").ok));
    REQUIRE(run("use gem blue_box").ok);
    for (int i = 0; i < 2000 && !game.inCutscene(); ++i) game.tick(0.05F);
    REQUIRE(game.inCutscene()); // the first say, which is not the last step
    result = run("use brass_key red_box");
    REQUIRE((!result.ok && result.output == "Sequencia em andamento; tente de novo depois"));
    settle();

    // No rule for that pair, and the player is already at blue_box (the gem): nothing ran.
    result = run("use brass_key blue_box");
    REQUIRE(!result.ok);
    REQUIRE(result.output
            == "Nada aconteceu: nenhuma regra [use_item brass_key blue_box] se aplica, ou o "
               "objeto esta fora de alcance");
}

// goto leaves through RoomChangeRequested (the change_room door) and keeps the progress.
TEST_CASE("testGotoCommandKeepsSession") {
    ShippedGame shipped;
    Game& game = *shipped.game;
    REQUIRE(shipped.execute("give brass_key").ok);
    REQUIRE(shipped.execute("flag visits 2").ok);
    REQUIRE(shipped.execute("hideobj blue_box").ok);
    const SessionSnapshot before = game.session().snapshot();

    const CommandResult result = shipped.execute("goto gallery");
    REQUIRE((result.ok && result.output == "Indo para gallery"));
    REQUIRE(pendingRoomChange(game, "gallery", ""));
    REQUIRE(game.scene().room().id == "first"); // the shell loads it, not the command
    REQUIRE(shipped.script->settle());
    REQUIRE(game.scene().room().id == "gallery");
    REQUIRE(shipped.printedLine("sala: gallery"));
    REQUIRE(game.session().snapshot() == before);
    REQUIRE(!game.scene().interactions().rules.empty()); // gallery.rules came with it

    REQUIRE(shipped.script->run("goto first"));
    REQUIRE((game.scene().room().id == "first" && !game.scene().isObjectVisible("blue_box")));
    REQUIRE(game.session().snapshot() == before);

    // Refused targets change nothing and ask for nothing.
    CommandResult refused = shipped.execute("goto nowhere");
    REQUIRE((!refused.ok && refused.output.starts_with("Sala desconhecida: nowhere (")));
    refused = shipped.execute("goto gallery window");
    REQUIRE((!refused.ok && refused.output == "Spawn desconhecido na sala gallery: window"));
    for (const char* line : {"goto", "goto sala/1", "goto gallery porta!", "goto a b c"}) {
        REQUIRE(shipped.execute(line).output == "Use: goto <sala> [spawn]");
    }
    REQUIRE(!hasRoomChange(game));
    CommandConsole withoutLibrary;
    REQUIRE(withoutLibrary.execute("goto gallery", game).output
            == "Pasta de salas nao configurada");
    REQUIRE(!hasRoomChange(game));
    REQUIRE(shipped.script->run("goto first")); // the room in play: re-entered at its start
    REQUIRE(game.session().snapshot() == before);
}

// A spawn travels with the request; the room is entered there (Room::entryPoint).
TEST_CASE("testGotoCommandSpawn") {
    const TempDirectory testDirectory("pointclick-editor-tests");
    const std::filesystem::path rooms = testDirectory.path() / "rooms";
    std::filesystem::create_directory(rooms);
    const RoomLibrary library = openLibrary(rooms);
    std::string error;
    Room hall = RoomPresets::firstRoom();
    hall.spawns = {{"door_left", {120.0F, 520.0F}, Facing::right}};
    REQUIRE(library.saveRoom("hall.room", hall, error));
    Game game(RoomPresets::firstRoom());
    CommandConsole console;
    console.setLibrary(library);
    const CommandResult result = console.execute("goto hall door_left", game);
    REQUIRE((result.ok && result.output == "Indo para hall em door_left"));
    REQUIRE(pendingRoomChange(game, "hall", "door_left"));
    RoomBundleResult loaded = library.loadBundle("hall.room");
    REQUIRE(loaded.bundle);
    REQUIRE(enterRoom(game, std::move(*loaded.bundle), "door_left", error));
    REQUIRE(game.scene().player().facing() == Facing::right);
    REQUIRE(game.scene().player().feet().x == 120.0F);
}

TEST_CASE("testNewGameCommand") {
    ShippedGame shipped;
    Game& game = *shipped.game;
    REQUIRE(shipped.execute("give brass_key").ok);
    REQUIRE(shipped.execute("flag visits 2").ok);
    REQUIRE(shipped.execute("hideobj gallery.plant").ok);
    REQUIRE(shipped.script->run("use brass_key red_box"));
    REQUIRE(game.session().hasFlag("red_box_unlocked"));
    REQUIRE(game.scene().navigation().canStandAt({860.0F, 575.0F})); // the box was unlocked
    REQUIRE(shipped.script->run("goto gallery"));

    // A start room that does not load costs nothing.
    shipped.console.setStartRoom("nowhere");
    CommandResult result = shipped.execute("newgame");
    REQUIRE((!result.ok && result.output.starts_with("Sala desconhecida: nowhere (")));
    REQUIRE((game.session().hasFlag("red_box_unlocked") && !hasRoomChange(game)));
    REQUIRE(shipped.execute("newgame now").output == "Use: newgame");

    // In the middle of a sequence: it ends at once, and the progress is gone.
    shipped.console.setStartRoom("first");
    REQUIRE(game.startDialogue("mesa"));
    REQUIRE(game.inCutscene());
    result = shipped.execute("newgame");
    REQUIRE((result.ok && result.output == "Novo jogo: progresso zerado, voltando a first"));
    REQUIRE(!game.inCutscene());
    REQUIRE(game.session().snapshot() == SessionSnapshot{});
    REQUIRE(pendingRoomChange(game, "first", ""));
    REQUIRE(shipped.script->settle());
    REQUIRE(game.scene().room().id == "first");
    REQUIRE(!game.scene().navigation().canStandAt({860.0F, 575.0F})); // locked again
    REQUIRE(game.scene().player().feet().x == game.scene().room().playerStart.x);
}

TEST_CASE("testStateCommand") {
    Game game(RoomPresets::firstRoom());
    CommandConsole console;
    CommandResult result = console.execute("state", game);
    REQUIRE((result.ok
             && result.output == "sala: first\nflags: -\nitens: -\nobjetos: -\nescolhas: -"));
    // Inserted out of order: flags, overrides and choices print sorted; items keep the
    // inventory order (the order the game shows them in).
    game.changeSession([](SessionState& session) {
        session.setFlag("zeta", -4);
        session.setFlag("alpha", 1);
        (void)session.addItem("rope");
        (void)session.addItem("apple");
        session.setObjectAnimation("gallery", "door", "open");
        session.setObjectSolid("first", "red_box", false);
        session.setObjectVisible("first", "blue_box", false);
        session.markDialogueChoice("gallery", "mesa", "sair");
        session.markDialogueChoice("gallery", "mesa", "abrir");
    });
    result = console.execute("state", game);
    REQUIRE(result.ok);
    REQUIRE(result.output
            == "sala: first\n"
               "flags: alpha=1, zeta=-4\n"
               "itens: rope, apple\n"
               "objetos: first.blue_box visible=0, first.red_box solid=0, gallery.door "
               "animation=open\n"
               "escolhas: gallery/mesa/abrir, gallery/mesa/sair");
    REQUIRE(console.execute("state", game).output == result.output);
    REQUIRE(console.execute("state all", game).output == "Use: state");
}

// Found in a real window: `state` (5 lines) lost its last line and `help` was cut mid-word
// with no sign. The panel keeps 5 status rows and marks what did not fit.
TEST_CASE("testPanelTextRows") {
    const std::vector<std::string> state = wrapPanelText(
            "sala: first\nflags: -\nitens: -\nobjetos: -\nescolhas: -", 5);
    REQUIRE(state.size() == 5);
    REQUIRE((state[0] == "sala: first" && state[4] == "escolhas: -"));

    const std::string longLine = "Comandos: " + commandNames(); // wider than one row
    const std::vector<std::string> list = wrapPanelText(longLine, 10);
    REQUIRE(list.size() > 1);
    std::string joined;
    for (const std::string& row : list) {
        REQUIRE(row.size() <= kPanelColumns);
        joined += (joined.empty() ? "" : " ") + row;
    }
    REQUIRE(joined == longLine); // broken at spaces, nothing lost

    Game game(RoomPresets::firstRoom());
    CommandConsole console;
    const std::vector<std::string> help = wrapPanelText(console.execute("help", game).output, 5);
    REQUIRE(help.size() == 5);
    REQUIRE((help.back().ends_with("...") && help.back().size() <= kPanelColumns));
    REQUIRE(wrapPanelText("", 5) == std::vector<std::string>{""});
    REQUIRE(wrapPanelText("a\nb\nc", 2) == std::vector<std::string>{"a", "b..."});
}
}
