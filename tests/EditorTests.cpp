#include "content/DialogueFiles.hpp"
#include "content/InteractionFiles.hpp"
#include "content/RoomFiles.hpp"
#include "support/RoomPresets.hpp"
#include "game/ActionCatalog.hpp"
#include "game/RoomParameters.hpp"
#include "content/ActorFiles.hpp"
#include "content/RoomLibrary.hpp"
#include "debug/DebugEditor.hpp"
#include "debug/DebugOverlay.hpp"
#include "game/Game.hpp"
#include "graphics/Renderer.hpp"
#include "input/InputRouter.hpp"
#include "input/InputSystem.hpp"
#include "ui/DialogueMenu.hpp"

#include <SDL3/SDL.h>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>

namespace {
// The shipped assets declare player art once, in assets/actors/player.actor.
const AnimationSet* sharedPlayerArt() {
    static const AnimationSet art = [] {
        const auto loaded = ActorFiles::load(std::filesystem::path(POINTCLICK_SOURCE_DIR)
                                             / "assets/actors/player.actor");
        assert(loaded.animations);
        return *loaded.animations;
    }();
    return &art;
}

RoomLibrary openLibrary(const std::filesystem::path& roomsDirectory) {
    RoomLibrary library;
    std::string error;
    assert(library.open(roomsDirectory, error));
    return library;
}

void command(DebugEditor& editor, Game& game, const std::string_view text) {
    assert(editor.onTextInput(text, game));
    assert(editor.onToolAction(ToolAction::submit, game));
}

void testEditing(const std::filesystem::path& directory) {
    Game game(RoomPresets::firstRoom());
    DebugEditor editor;
    const RoomLibrary library = openLibrary(directory);
    editor.setLibrary(library);
    assert(!editor.onToolAction(ToolAction::increase, game));
    assert(!editor.onTextInput("DE", game));
    assert(!editor.active());
    assert(editor.onTextInput("bug", game));
    assert(editor.active());
    editor.onToolAction(ToolAction::increase, game);
    assert(game.scene().room().playerFarSpeed == 120);
    command(editor, game, "set player.near_speed 400");
    assert(game.scene().room().playerNearSpeed == 400);

    game.onClick({700, 400});
    const auto before = game.scene().player().destination();
    command(editor, game, "set perspective.near_y 100");
    assert(game.scene().room().nearDepthY == 720);
    assert(game.scene().player().moving()); // Rejected changes preserve the whole simulation.
    assert(game.scene().player().destination().x == before.x);
    command(editor, game, "set player.width -1");
    assert(game.scene().room().playerBaseSize.x == 48);
    command(editor, game, "set player.width nan");
    assert(game.scene().room().playerBaseSize.x == 48);

    command(editor, game, "save authored.room");
    command(editor, game, "set player.near_speed 500");
    command(editor, game, "undo");
    assert(game.scene().room().playerNearSpeed == 400);
    command(editor, game, "load authored.room");
    assert(game.scene().room().playerFarSpeed == 120);
    assert(!game.scene().player().moving());
    command(editor, game, "save ../escape.room");
    assert(editor.status().find("sem pastas") != std::string::npos);

    command(editor, game, "teleport 860 575");
    assert(game.scene().player().feet().x == 640); // Cannot place feet in the red base.
    command(editor, game, "move red_box 620 240");
    assert(game.scene().player().feet().x != 640 || game.scene().player().feet().y != 360);
    assert(game.scene().room().scenery[1].bounds.x == 620);
    command(editor, game, "move red_box 2000 240");
    assert(game.scene().room().scenery[1].bounds.x == 620);
    command(editor, game, "set room.width 1000");
    assert(game.scene().room().width == 1280);
    command(editor, game, "set room.height 600");
    assert(game.scene().room().height == 720);
    command(editor, game, "set room.width 2560");
    assert(game.scene().room().width == 2560);
    command(editor, game, "move red_box 2000 240");
    assert(game.scene().room().scenery[1].bounds.x == 2000);
    command(editor, game, "set room.width 1280");
    assert(game.scene().room().width == 2560); // Cannot shrink past an object at x = 2000.
    command(editor, game, "reset");
    assert(game.scene().room().width == 1280);
    assert(game.scene().room().playerFarSpeed == 110);
    assert(game.scene().room().scenery[1].bounds.x == 810);

    editor.onToolAction(ToolAction::togglePanel, game);
    assert(editor.active() && !editor.panelVisible());
    editor.onToolAction(ToolAction::increase, game);
    assert(game.scene().room().playerFarSpeed == 110);
    assert(editor.onTextInput("set player.far_speed 500", game));
    assert(editor.command().empty());
    editor.onToolAction(ToolAction::togglePanel, game);
    assert(editor.panelVisible());

    editor.onToolAction(ToolAction::close, game);
    assert(!editor.active());
    assert(!editor.onToolAction(ToolAction::increase, game));
    assert(game.scene().room().playerFarSpeed == 110);

    Room blocked = game.scene().room();
    blocked.scenery = {{{0, 0, 1280, 720}, {255, 0, 0, 255}, SceneObjectType::solid,
                       {0, 0, 1280, 720}, "wall", {}}};
    std::string error;
    assert(!game.applyRoom(blocked, false, error));
    assert(game.scene().room().scenery.size() == 2);
}

void testRoomFiles(const std::filesystem::path& directory) {
    const auto source = std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets/rooms/first.room";
    const auto loaded = RoomFiles::load(source, sharedPlayerArt());
    assert(loaded.room);
    assert(loaded.room->width == 1280.0F && loaded.room->height == 720.0F);
    assert(loaded.room->scenery.size() == 2);
    const auto missing = RoomFiles::load(directory / std::filesystem::path(u8"ausente-\u00e7.room"));
    assert(!missing.room);
    assert(missing.error == "Nao foi possivel abrir ausente-\xC3\xA7.room");

    const auto unicodeDirectory = directory / std::filesystem::path(u8"salas-a\u00e7\u00e3o");
    std::filesystem::create_directory(unicodeDirectory);
    const auto commentedPath = unicodeDirectory / std::filesystem::path(u8"sala-\u65e5.room");
    {
        std::ofstream out(commentedPath, std::ios::binary);
        out << "# comentario\r\n[room] # secao\r\n\r\nid = commented # id\r\n"
               "player.far_speed = 123.5 # velocidade";
    }
    const auto commented = RoomFiles::load(commentedPath, sharedPlayerArt());
    assert(commented.room && commented.error.empty());
    assert(commented.room->id == "commented");
    assert(commented.room->playerFarSpeed == 123.5F);

    const auto emptyPath = directory / "empty.room";
    { std::ofstream out(emptyPath, std::ios::binary); }
    const auto empty = RoomFiles::load(emptyPath, sharedPlayerArt());
    assert(empty.room && empty.error.empty());
    assert(empty.room->id == "empty"); // no `id` key: named after the file, not "first"

    const auto nulPath = directory / "nul.room";
    {
        std::ofstream out(nulPath, std::ios::binary);
        out << "[room]\nid = nul_room\n";
        out.put('\0');
        out << '\n';
    }
    const auto nul = RoomFiles::load(nulPath);
    assert(!nul.room && nul.error == "Linha 3: Esperado chave = valor");

    const auto gallery = RoomFiles::load(source.parent_path() / "gallery.room", sharedPlayerArt());
    assert(gallery.room);
    Game other(*gallery.room);
    assert(other.scene().room().id == "gallery");
    other.onClick({400, 460});
    assert(other.scene().player().moving());
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
    assert(RoomFiles::save(path, room, error));
    const auto restored = RoomFiles::load(path);
    assert(restored.room);
    for (const auto& parameter : roomParameters()) {
        assert(parameter.read(room) == parameter.read(*restored.room));
    }
    assert(restored.room->id == room.id);
    assert(restored.room->width == 2560.0F);
    assert(restored.room->height == 1440.0F);
    assert(restored.room->floorColor.b == room.floorColor.b);
    assert(restored.room->scenery[1].bounds.x == 2100.0F);
    assert(restored.room->scenery[1].collisionFootprint.h == 21.5F);
    assert(restored.room->scenery[1].id == "red_box");

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
        assert(!result.room && result.error == invalid.error);
    }
    const auto validSpeed = restored.room->playerNearSpeed;
    room.nearDepthY = room.farDepthY;
    assert(!RoomFiles::save(path, room, error));
    assert(RoomFiles::load(path).room->playerNearSpeed == validSpeed);
}

bool surfaceContainsColor(SDL_Surface* surface, const SDL_Color target) {
    for (int y = 0; y < surface->h; y += 8) {
        for (int x = 0; x < surface->w; x += 8) {
            SDL_Color color{};
            assert(SDL_ReadSurfacePixel(surface, x, y, &color.r, &color.g, &color.b, &color.a));
            if (color.r == target.r && color.g == target.g && color.b == target.b) {
                return true;
            }
        }
    }
    return false;
}

void testDebugRendering() {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window* window = SDL_CreateWindow("Editor test", 1280, 720, SDL_WINDOW_HIDDEN);
    assert(window);
    Renderer renderer;
    assert(renderer.initialize(window));
    Game game(RoomPresets::firstRoom());
    DebugEditor editor;
    const DebugOverlay overlay(editor);
    renderer.addOverlay(&overlay);
    editor.onTextInput("DEBUG", game);
    renderer.render(game, 1.0F);
    SDL_Surface* pixels = SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr);
    assert(pixels);
    if (const char* capture = std::getenv("POINTCLICK_DEBUG_CAPTURE")) assert(SDL_SaveBMP(pixels, capture));
    SDL_DestroySurface(pixels);
    // Resizing preserves the logical center used by the mouse and editor.
    assert(SDL_SetWindowSize(window, 1000, 1000));
    SDL_PumpEvents();
    renderer.render(game, 1.0F);
    SDL_FPoint center{};
    assert(renderer.windowToWorld(500, 500, center));
    assert(std::abs(center.x - 640) < 0.1F && std::abs(center.y - 360) < 0.1F);
    SDL_FPoint bar{};
    assert(renderer.windowToWorld(500, 20, bar));
    assert(bar.y < 0);
    assert(!Renderer::containsViewportPoint(bar));

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
    assert(std::abs(game.scene().camera().position().x) < 0.01F);

    const struct { int w; int h; } resolutions[]{
        {1280, 720},
        {1920, 1080},
        {2560, 1440}, // 2K QHD
        {3840, 2160}, // 4K UHD
        {3440, 1440}, // 21:9 Ultrawide (pillarboxed)
    };
    for (const auto res : resolutions) {
        assert(SDL_SetWindowSize(window, res.w, res.h));
        SDL_PumpEvents();
        renderer.render(game, 1.0F, nullptr);

        SDL_FRect presentation{};
        assert(SDL_GetRenderLogicalPresentationRect(SDL_GetRenderer(window), &presentation));
        SDL_FPoint vpCenter{};
        SDL_FPoint worldCenter{};
        assert(renderer.windowToViewport(res.w * 0.5F, res.h * 0.5F, vpCenter));
        assert(renderer.windowToWorld(res.w * 0.5F, res.h * 0.5F, worldCenter,
                                      game.scene().camera().position()));
        assert(std::abs(vpCenter.x - 640.0F) < 0.5F && std::abs(vpCenter.y - 360.0F) < 0.5F);
        assert(std::abs(worldCenter.x - 640.0F) < 0.5F && std::abs(worldCenter.y - 360.0F) < 0.5F);

        SDL_FPoint leftEdge{};
        SDL_FPoint rightEdge{};
        assert(renderer.windowToWorld(presentation.x + 0.5F, res.h * 0.5F, leftEdge,
                                      game.scene().camera().position()));
        assert(renderer.windowToWorld(presentation.x + presentation.w - 0.5F, res.h * 0.5F,
                                      rightEdge, game.scene().camera().position()));
        assert(leftEdge.x >= 0.0F && rightEdge.x <= 1280.0F);
        assert(std::abs((rightEdge.x - leftEdge.x) - 1280.0F) < 2.0F);

        SDL_Surface* frame = SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr);
        assert(frame);
        // The beacon at x = 1500..1720 must remain completely invisible while camera.x == 0.
        assert(!surfaceContainsColor(frame, offscreenBeaconColor));
        SDL_DestroySurface(frame);
    }

    // Once the camera scrolls to center on x = 1550 (camera.x = 910), the beacon enters view
    // while Ultrawide pillarbox bars remain outside the viewport and keep the clear color.
    std::string teleportError;
    assert(game.teleportPlayer({1550.0F, 620.0F}, teleportError));
    assert(std::abs(game.scene().camera().position().x - 910.0F) < 0.01F);
    assert(SDL_SetWindowSize(window, 3440, 1440));
    SDL_PumpEvents();
    renderer.render(game, 1.0F, nullptr);

    SDL_FPoint ultrawideBar{};
    assert(renderer.windowToViewport(10.0F, 720.0F, ultrawideBar));
    assert(ultrawideBar.x < 0.0F && !Renderer::containsViewportPoint(ultrawideBar));

    SDL_FPoint scrolledCenterViewport{};
    SDL_FPoint scrolledCenterWorld{};
    assert(renderer.windowToViewport(1720.0F, 720.0F, scrolledCenterViewport));
    assert(renderer.windowToWorld(1720.0F, 720.0F, scrolledCenterWorld,
                                  game.scene().camera().position()));
    assert(std::abs(scrolledCenterViewport.x - 640.0F) < 0.5F);
    assert(std::abs(scrolledCenterWorld.x - 1550.0F) < 0.5F);

    SDL_Surface* scrolledFrame = SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr);
    assert(scrolledFrame);
    assert(scrolledFrame->w == 2560 && scrolledFrame->h == 1440);
    assert(surfaceContainsColor(scrolledFrame, offscreenBeaconColor));
    SDL_DestroySurface(scrolledFrame);

    if (SDL_Surface* windowSurface = SDL_GetWindowSurface(window)) {
        assert(windowSurface->w == 3440 && windowSurface->h == 1440);
        SDL_Color leftBar{};
        SDL_Color rightBar{};
        assert(SDL_ReadSurfacePixel(windowSurface, 10, 720,
                                    &leftBar.r, &leftBar.g, &leftBar.b, &leftBar.a));
        assert(SDL_ReadSurfacePixel(windowSurface, 3430, 720,
                                    &rightBar.r, &rightBar.g, &rightBar.b, &rightBar.a));
        assert(leftBar.r == 8 && leftBar.g == 10 && leftBar.b == 16);
        assert(rightBar.r == 8 && rightBar.g == 10 && rightBar.b == 16);
    }

    renderer.shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void testMultiplatformInputAndRouting() {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window* window = SDL_CreateWindow("Input routing test", 3440, 1440, SDL_WINDOW_HIDDEN);
    assert(window);
    Renderer renderer;
    assert(renderer.initialize(window));
    assert(SDL_SetWindowSize(window, 3440, 1440));
    SDL_PumpEvents();

    // Panoramic room (2560x720) with no obstacles so every floor point is walkable.
    Room panorama = RoomPresets::firstRoom();
    panorama.width = 2560.0F;
    panorama.height = 720.0F;
    panorama.scenery.clear();
    panorama.playerStart = {1550.0F, 520.0F}; // Camera snaps to x = 910 (1550 - 640).
    Game game(panorama);
    assert(std::abs(game.scene().camera().position().x - 910.0F) < 0.01F);
    renderer.render(game, 1.0F, nullptr);

    DebugEditor editor;
    InputSystem input;
    InputRouter router;
    router.setToolConsumer(&editor);
    const ViewportCoordinateMapper mapper = renderer.viewportMapper();

    // 1. DebugEditor panel hit-test: blocks clicks when visible, releases when hidden or closed.
    assert(!editor.consumesViewportPoint({900.0F, 500.0F}));
    assert(editor.onTextInput("DEBUG", game));
    assert(editor.active() && editor.panelVisible());
    assert(editor.consumesViewportPoint({900.0F, 500.0F}));
    assert(!editor.consumesViewportPoint({300.0F, 500.0F}));

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
    assert(routeResult.consumedByDebug);
    assert(!game.scene().player().moving());

    // Clicking outside the panel at Viewport (300, 520) -> World (1210, 520) walks the player.
    SDL_Event outsidePanelClick = panelClick;
    outsidePanelClick.button.x = 440.0F + 300.0F * 2.0F;
    outsidePanelClick.button.y = 520.0F * 2.0F;
    routeResult = router.handleEvent(outsidePanelClick, input, game, mapper);
    assert(!routeResult.consumedByDebug);
    assert(game.scene().player().moving());
    assert(std::abs(game.scene().player().destination().x - 1210.0F) < 0.5F);
    assert(std::abs(game.scene().player().destination().y - 520.0F) < 0.5F);

    std::string teleportError;
    assert(game.teleportPlayer({1550.0F, 520.0F}, teleportError));

    // Hiding the panel with TAB allows clicking at Viewport (900, 500) -> World (1810, 500).
    SDL_Event tabKey{};
    tabKey.type = SDL_EVENT_KEY_DOWN;
    tabKey.key.key = SDLK_TAB;
    tabKey.key.down = true;
    assert(router.handleEvent(tabKey, input, game, mapper).consumedByDebug);
    assert(editor.active() && !editor.panelVisible());
    assert(!editor.consumesViewportPoint({900.0F, 500.0F}));

    routeResult = router.handleEvent(panelClick, input, game, mapper);
    assert(!routeResult.consumedByDebug);
    assert(game.scene().player().moving());
    assert(std::abs(game.scene().player().destination().x - 1810.0F) < 0.5F);
    assert(std::abs(game.scene().player().destination().y - 500.0F) < 0.5F);

    assert(game.teleportPlayer({1550.0F, 520.0F}, teleportError));

    // Closing the editor with ESC also leaves the panel area unblocked.
    SDL_Event escKey{};
    escKey.type = SDL_EVENT_KEY_DOWN;
    escKey.key.key = SDLK_ESCAPE;
    escKey.key.down = true;
    assert(router.handleEvent(escKey, input, game, mapper).consumedByDebug);
    assert(!editor.active());
    routeResult = router.handleEvent(panelClick, input, game, mapper);
    assert(!routeResult.consumedByDebug);
    assert(game.scene().player().moving());
    assert(std::abs(game.scene().player().destination().x - 1810.0F) < 0.5F);

    assert(game.teleportPlayer({1550.0F, 520.0F}, teleportError));

    // 2. Touch translation + letterbox/pillarbox rejection + SDL_TOUCH_MOUSEID filtering.
    // Touch inside left Ultrawide pillarbox bar (normalized x = 0.02 -> window x = 68.8 < 440).
    SDL_Event pillarboxTouch{};
    pillarboxTouch.type = SDL_EVENT_FINGER_DOWN;
    pillarboxTouch.tfinger.touchID = 10;
    pillarboxTouch.tfinger.fingerID = 1;
    pillarboxTouch.tfinger.x = 0.02F;
    pillarboxTouch.tfinger.y = 0.75F;
    (void)router.handleEvent(pillarboxTouch, input, game, mapper);
    assert(!game.scene().player().moving());
    assert(!game.scene().pointerInside());

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
    assert(input.activeDevice() == InputDevice::touch);
    assert(game.scene().player().moving());
    assert(std::abs(game.scene().player().destination().x - 1710.0F) < 0.5F); // 800 + 910
    assert(std::abs(game.scene().player().destination().y - 540.0F) < 0.5F);

    // Synthetic mouse motion & click emitted by touch must be ignored (no double-click / retarget).
    SDL_Event syntheticMouseClick{};
    syntheticMouseClick.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    syntheticMouseClick.button.which = SDL_TOUCH_MOUSEID;
    syntheticMouseClick.button.button = SDL_BUTTON_LEFT;
    syntheticMouseClick.button.down = true;
    syntheticMouseClick.button.x = 440.0F + 200.0F * 2.0F;
    syntheticMouseClick.button.y = 600.0F * 2.0F;
    (void)router.handleEvent(syntheticMouseClick, input, game, mapper);
    assert(input.activeDevice() == InputDevice::touch);
    assert(std::abs(game.scene().player().destination().x - 1710.0F) < 0.5F);

    SDL_Event syntheticMouseTouch{};
    syntheticMouseTouch.type = SDL_EVENT_FINGER_DOWN;
    syntheticMouseTouch.tfinger.touchID = SDL_MOUSE_TOUCHID;
    syntheticMouseTouch.tfinger.fingerID = 3;
    syntheticMouseTouch.tfinger.x = 0.3F;
    syntheticMouseTouch.tfinger.y = 0.8F;
    (void)router.handleEvent(syntheticMouseTouch, input, game, mapper);
    assert(std::abs(game.scene().player().destination().x - 1710.0F) < 0.5F);

    SDL_Event worldTouchUp = worldTouch;
    worldTouchUp.type = SDL_EVENT_FINGER_UP;
    (void)router.handleEvent(worldTouchUp, input, game, mapper);

    assert(game.teleportPlayer({1550.0F, 520.0F}, teleportError));

    // 3. Gamepad hotplug, deadzone, virtual cursor movement, and scrolled world click.
    SDL_Event padAdded{};
    padAdded.type = SDL_EVENT_GAMEPAD_ADDED;
    padAdded.gdevice.which = 77;
    (void)router.handleEvent(padAdded, input, game, mapper);
    assert(input.gamepadCount() == 1);

    input.setCursorViewport({640.0F, 360.0F});
    input.setCursorSpeed(400.0F);

    // Sub-deadzone stick deflection must produce zero movement.
    SDL_Event smallTilt{};
    smallTilt.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    smallTilt.gaxis.which = 77;
    smallTilt.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTX;
    smallTilt.gaxis.value = 3000; // ~0.09 < 0.24 deadzone
    (void)router.handleEvent(smallTilt, input, game, mapper);
    assert(input.moveAxis().x == 0.0F && input.moveAxis().y == 0.0F);
    router.update(0.5F, input, game);
    assert(std::abs(input.cursorViewport().x - 640.0F) < 0.01F);

    // Full stick deflection right & down moves the virtual cursor smoothly in viewport space.
    SDL_Event stickRight = smallTilt;
    stickRight.gaxis.value = 32767;
    (void)router.handleEvent(stickRight, input, game, mapper);
    assert(std::abs(input.moveAxis().x - 1.0F) < 0.01F);

    SDL_Event stickDown = smallTilt;
    stickDown.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTY;
    stickDown.gaxis.value = 32767;
    (void)router.handleEvent(stickDown, input, game, mapper);
    assert(std::hypot(input.moveAxis().x, input.moveAxis().y) <= 1.001F);

    // Zero Y, move purely right by 200px (0.5s * 400px/s) and down by 100px: cursor -> (840, 460).
    stickDown.gaxis.value = 0;
    (void)router.handleEvent(stickDown, input, game, mapper);
    router.update(0.5F, input, game);
    assert(std::abs(input.cursorViewport().x - 840.0F) < 0.1F);

    stickRight.gaxis.value = 0;
    (void)router.handleEvent(stickRight, input, game, mapper);
    stickDown.gaxis.value = 32767;
    (void)router.handleEvent(stickDown, input, game, mapper);
    router.update(0.25F, input, game);
    assert(std::abs(input.cursorViewport().y - 460.0F) < 0.1F);

    // Primary gamepad button (SOUTH) clicks at virtual cursor (840, 460) -> World (1750, 460).
    SDL_Event southPress{};
    southPress.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    southPress.gbutton.which = 77;
    southPress.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
    southPress.gbutton.down = true;
    (void)router.handleEvent(southPress, input, game, mapper);
    assert(input.activeDevice() == InputDevice::gamepad);
    assert(game.scene().player().moving());
    assert(std::abs(game.scene().player().destination().x - 1750.0F) < 0.5F); // 840 + 910
    assert(std::abs(game.scene().player().destination().y - 460.0F) < 0.5F);

    // Virtual cursor clamps cleanly at viewport boundaries [0..1279] x [0..719].
    router.update(10.0F, input, game);
    assert(Renderer::containsViewportPoint(input.cursorViewport()));
    assert(std::abs(input.cursorViewport().y - 719.0F) < 0.01F);

    // Unplugging the gamepad resets held stick axes to prevent drift.
    SDL_Event padRemoved{};
    padRemoved.type = SDL_EVENT_GAMEPAD_REMOVED;
    padRemoved.gdevice.which = 77;
    (void)router.handleEvent(padRemoved, input, game, mapper);
    assert(input.gamepadCount() == 0);
    assert(input.moveAxis().x == 0.0F && input.moveAxis().y == 0.0F);

    // 4. DebugEditor open/navigate/adjust via Keyboard (F1 + arrows) and Gamepad (BACK + D-Pad/Shoulders).
    SDL_Event f1Key{};
    f1Key.type = SDL_EVENT_KEY_DOWN;
    f1Key.key.key = SDLK_F1;
    f1Key.key.down = true;
    assert(router.handleEvent(f1Key, input, game, mapper).consumedByDebug);
    assert(editor.active() && editor.panelVisible());

    SDL_Event padBack{};
    padBack.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    padBack.gbutton.which = 77;
    padBack.gbutton.button = SDL_GAMEPAD_BUTTON_BACK;
    padBack.gbutton.down = true;
    assert(router.handleEvent(padBack, input, game, mapper).consumedByDebug);
    assert(!editor.active()); // Toggled closed by gamepad BACK.
    assert(router.handleEvent(padBack, input, game, mapper).consumedByDebug);
    assert(editor.active() && editor.panelVisible()); // Re-opened without typing "DEBUG"!

    const float initialFarSpeed = game.scene().room().playerFarSpeed;
    assert(editor.selected() == 0);

    SDL_Event dpadRight = padBack;
    dpadRight.gbutton.button = SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    assert(router.handleEvent(dpadRight, input, game, mapper).consumedByDebug);
    assert(game.scene().room().playerFarSpeed > initialFarSpeed);

    SDL_Event leftShoulder = padBack;
    leftShoulder.gbutton.button = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    assert(router.handleEvent(leftShoulder, input, game, mapper).consumedByDebug);
    assert(std::abs(game.scene().room().playerFarSpeed - initialFarSpeed) < 0.01F);

    SDL_Event dpadDown = padBack;
    dpadDown.gbutton.button = SDL_GAMEPAD_BUTTON_DPAD_DOWN;
    assert(router.handleEvent(dpadDown, input, game, mapper).consumedByDebug);
    assert(editor.selected() == 1);

    SDL_Event keyUp{};
    keyUp.type = SDL_EVENT_KEY_DOWN;
    keyUp.key.key = SDLK_UP;
    keyUp.key.down = true;
    assert(router.handleEvent(keyUp, input, game, mapper).consumedByDebug);
    assert(editor.selected() == 0);

    SDL_Event padWest = padBack;
    padWest.gbutton.button = SDL_GAMEPAD_BUTTON_WEST;
    assert(router.handleEvent(padWest, input, game, mapper).consumedByDebug);
    assert(editor.active() && !editor.panelVisible());
    assert(router.handleEvent(padWest, input, game, mapper).consumedByDebug);
    assert(editor.panelVisible());

    SDL_Event padEast = padBack;
    padEast.gbutton.button = SDL_GAMEPAD_BUTTON_EAST;
    assert(router.handleEvent(padEast, input, game, mapper).consumedByDebug);
    assert(!editor.active());

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

    assert(game.teleportPlayer({1550.0F, 520.0F}, teleportError));
    SDL_Event modalClick = panelClick;
    modalClick.button.x = 440.0F + 300.0F * 2.0F; // Viewport (300, 300) inside modalBox
    modalClick.button.y = 300.0F * 2.0F;
    routeResult = router.handleEvent(modalClick, input, game, mapper);
    assert(routeResult.consumedByUi);
    assert(!game.scene().player().moving());

    routeResult = router.handleEvent(dpadDown, input, game, mapper);
    assert(routeResult.consumedByUi && modalUi.navCount == 1);
    router.clearUiConsumers();

    // Lifecycle signals: background clears transient axes, device reset signals invalidation.
    stickRight.gaxis.value = 32767;
    (void)router.handleEvent(stickRight, input, game, mapper);
    assert(input.moveAxis().x > 0.5F);

    SDL_Event bgEvent{};
    bgEvent.type = SDL_EVENT_WILL_ENTER_BACKGROUND;
    routeResult = router.handleEvent(bgEvent, input, game, mapper);
    assert(routeResult.enteredBackground);
    assert(input.moveAxis().x == 0.0F && input.moveAxis().y == 0.0F);

    SDL_Event fgEvent{};
    fgEvent.type = SDL_EVENT_DID_ENTER_FOREGROUND;
    assert(router.handleEvent(fgEvent, input, game, mapper).enteredForeground);

    SDL_Event resetEvent{};
    resetEvent.type = SDL_EVENT_RENDER_DEVICE_RESET;
    assert(router.handleEvent(resetEvent, input, game, mapper).invalidateGraphics);
    renderer.invalidateTextures();

    SDL_Event quitEvent{};
    quitEvent.type = SDL_EVENT_QUIT;
    assert(router.handleEvent(quitEvent, input, game, mapper).quitRequested);

    renderer.shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void testEditorPreservesSessionState(const std::filesystem::path& directory) {
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
    assert(game.session().hasFlag("found_brass_key"));
    assert(game.session().hasItem("brass_key"));

    assert(game.useItemOnObject("brass_key", "red_box"));
    for (int i = 0; i < 100; ++i) game.tick(0.05F);
    assert(!game.session().hasItem("brass_key"));
    assert(game.session().hasFlag("red_box_unlocked"));
    assert(!game.scene().isObjectSolid(game.scene().room().scenery[1]));
    assert(game.scene().navigation().canStandAt({860.0F, 575.0F}));

    game.onClick({530.0F, 510.0F});
    for (int i = 0; i < 80; ++i) game.tick(0.05F);
    assert(game.session().hasItem("magenta_gem"));
    assert(!game.scene().isObjectVisible("magenta_gem"));

    // 2. Live edits in DebugEditor (set, move, undo, reset) MUST preserve SessionState!
    DebugEditor editor;
    const RoomLibrary library = openLibrary(directory);
    editor.setLibrary(library);
    assert(editor.onTextInput("DEBUG", game));
    assert(editor.active());

    command(editor, game, "set player.far_speed 180");
    assert(game.scene().room().playerFarSpeed == 180.0F);
    assert(game.session().hasItem("magenta_gem"));
    assert(game.session().hasFlag("found_brass_key"));
    assert(game.session().hasFlag("red_box_unlocked"));
    assert(!game.scene().isObjectVisible("magenta_gem"));
    assert(!game.scene().isObjectSolid(game.scene().room().scenery[1]));
    assert(game.scene().navigation().canStandAt({860.0F, 575.0F}));

    command(editor, game, "move blue_box 300 350");
    assert(game.scene().room().scenery[0].bounds.x == 300.0F);
    assert(game.session().hasItem("magenta_gem"));
    assert(!game.scene().isObjectVisible("magenta_gem"));
    assert(game.scene().navigation().canStandAt({860.0F, 575.0F}));

    command(editor, game, "undo");
    assert(game.scene().room().scenery[0].bounds.x == 380.0F);
    assert(game.session().hasItem("magenta_gem"));
    assert(game.session().hasFlag("red_box_unlocked"));
    assert(!game.scene().isObjectVisible("magenta_gem"));
    assert(game.scene().navigation().canStandAt({860.0F, 575.0F}));

    command(editor, game, "reset");
    assert(game.scene().room().playerFarSpeed == 110.0F);
    assert(game.session().hasItem("magenta_gem"));
    assert(game.session().hasFlag("red_box_unlocked"));
    assert(!game.scene().isObjectVisible("magenta_gem"));
    assert(game.scene().navigation().canStandAt({860.0F, 575.0F}));
    editor.onToolAction(ToolAction::close, game);

    // 3. Headless SceneRenderer check: hidden magenta_gem is NOT drawn; showing it draws it again!
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window* window = SDL_CreateWindow("Visibility render test", 1280, 720, SDL_WINDOW_HIDDEN);
    assert(window);
    Renderer renderer;
    assert(renderer.initialize(window));

    renderer.render(game, 1.0F, nullptr);
    SDL_Surface* hiddenFrame = SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr);
    assert(hiddenFrame);
    assert(!surfaceContainsColor(hiddenFrame, pickupColor));
    SDL_DestroySurface(hiddenFrame);

    game.session().setObjectVisible(game.scene().room().id, "magenta_gem", true);
    renderer.render(game, 1.0F, nullptr);
    SDL_Surface* shownFrame = SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr);
    assert(shownFrame);
    assert(surfaceContainsColor(shownFrame, pickupColor));
    SDL_DestroySurface(shownFrame);

    renderer.shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void testInteractionRulesFilesAndValidation(const std::filesystem::path& directory) {
    // 1. Validate shipped companion .rules files against shipped .room files.
    const auto roomsDir = std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets/rooms";
    const auto firstRoom = RoomFiles::load(roomsDir / "first.room", sharedPlayerArt());
    assert(firstRoom.room);
    const auto firstRules = InteractionFiles::loadOptionalForRoom(
            roomsDir / "first.room", &*firstRoom.room);
    assert(firstRules.interactions && firstRules.error.empty());
    assert(!firstRules.interactions->empty());
    assert(firstRules.interactions->hasRulesForObject("blue_box"));
    assert(firstRules.interactions->hasRulesForObject("red_box"));

    const auto galleryRoom = RoomFiles::load(roomsDir / "gallery.room", sharedPlayerArt());
    assert(galleryRoom.room);
    const auto galleryRules = InteractionFiles::loadOptionalForRoom(
            roomsDir / "gallery.room", &*galleryRoom.room);
    assert(galleryRules.interactions && galleryRules.error.empty());
    assert(galleryRules.interactions->hasRulesForObject("plant"));
    assert(galleryRules.interactions->hasRulesForObject("table"));

    // 2. Optional companion loading succeeds with empty rules when no .rules exists.
    const auto lonelyRoomPath = directory / "no_rules_here.room";
    std::string saveError;
    assert(RoomFiles::save(lonelyRoomPath, *firstRoom.room, saveError));
    const auto optionalMissing = InteractionFiles::loadOptionalForRoom(
            lonelyRoomPath, &*firstRoom.room);
    assert(optionalMissing.interactions && optionalMissing.interactions->empty());
    assert(optionalMissing.error.empty());

    // Direct load on a missing .rules file returns a clear open error.
    const auto directMissing = InteractionFiles::load(directory / "missing.rules");
    assert(!directMissing.interactions);
    assert(directMissing.error == "Nao foi possivel abrir missing.rules");

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
    assert(parsed.interactions && parsed.error.empty());
    assert(parsed.interactions->rules.size() == 3);
    assert(parsed.interactions->rules[0].effects[2].args[0] == "Item #1: \"ficha dourada\" encontrada!");

    const auto rulesPath = directory / "roundtrip.rules";
    assert(InteractionFiles::save(rulesPath, *parsed.interactions, saveError, &testRoom));
    const auto reloaded = InteractionFiles::load(rulesPath, &testRoom);
    assert(reloaded.interactions && reloaded.error.empty());
    assert(reloaded.interactions->rules.size() == 3);
    assert(reloaded.interactions->rules[0].effects[2].args[0] == "Item #1: \"ficha dourada\" encontrada!");
    assert(reloaded.interactions->rules[1].trigger.kind == InteractionTriggerKind::useItem);
    assert(reloaded.interactions->rules[1].trigger.itemId == "golden_token");
    assert(reloaded.interactions->rules[1].trigger.objectId == "red_box");
    assert(reloaded.interactions->rules[1].effects.size() == 8);

    // 4. DebugEditor 'load' rejects a room if its companion .rules file has a syntax error,
    // preserving the active simulation!
    const auto brokenCompanionRoom = directory / "broken_rules.room";
    Room brokenRulesRoom = testRoom;
    brokenRulesRoom.id = "broken_rules"; // the library requires id == file name
    assert(RoomFiles::save(brokenCompanionRoom, brokenRulesRoom, saveError));
    {
        std::ofstream badCompanion(directory / "broken_rules.rules", std::ios::binary);
        badCompanion << "[interact blue_box]\nsay \"sem fechar aspas\n";
    }
    Game editorGame(testRoom);
    DebugEditor editor;
    const RoomLibrary library = openLibrary(directory);
    editor.setLibrary(library);
    assert(editor.onTextInput("DEBUG", editorGame));
    command(editor, editorGame, "set player.far_speed 155");
    assert(editorGame.scene().room().playerFarSpeed == 155.0F);
    command(editor, editorGame, "load broken_rules.room");
    assert(editor.status() == "Linha 2: Texto entre aspas nao fechado");
    assert(editorGame.scene().room().playerFarSpeed == 155.0F); // Rolled back!

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
        assert(!bad.interactions);
        assert(bad.error == testCase.expectedError);
    }
}

void assertSameSteps(const std::vector<InteractionEffect>& a, const std::vector<InteractionEffect>& b) {
    assert(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        assert(a[i].effect == b[i].effect && a[i].args == b[i].args);
    }
}

void assertSameDialogues(const RoomDialogues& a, const RoomDialogues& b) {
    assert(a.nodes.size() == b.nodes.size());
    for (std::size_t n = 0; n < a.nodes.size(); ++n) {
        const DialogueNode& x = a.nodes[n];
        const DialogueNode& y = b.nodes[n];
        assert(x.id == y.id && x.choices.size() == y.choices.size());
        assertSameSteps(x.lines, y.lines);
        for (std::size_t c = 0; c < x.choices.size(); ++c) {
            const DialogueChoice& p = x.choices[c];
            const DialogueChoice& q = y.choices[c];
            assert(p.id == q.id && p.text == q.text && p.once == q.once && p.next == q.next);
            assert(p.conditions.size() == q.conditions.size());
            for (std::size_t i = 0; i < p.conditions.size(); ++i) {
                assert(p.conditions[i].condition == q.conditions[i].condition);
                assert(p.conditions[i].args == q.conditions[i].args);
            }
            assertSameSteps(p.effects, q.effects);
        }
    }
}

void testDialogueFiles(const std::filesystem::path& directory) {
    // 1. The shipped gallery.dlg loads against gallery.room and matches gallery.rules.
    const auto roomsDir = std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets/rooms";
    const auto gallery = RoomFiles::load(roomsDir / "gallery.room", sharedPlayerArt());
    assert(gallery.room);
    const auto shipped = DialogueFiles::loadOptionalForRoom(roomsDir / "gallery.room",
                                                            &*gallery.room);
    assert(shipped.dialogues && shipped.error.empty());
    const DialogueNode* table = shipped.dialogues->findNode("mesa");
    assert(table && table->choices.size() == 3);
    assert(table->findChoice("ler_bilhete")->conditions.size() == 1);
    assert(table->findChoice("gavetas")->once && table->findChoice("gavetas")->next == "fundo_falso");
    const auto galleryRules = InteractionFiles::loadOptionalForRoom(roomsDir / "gallery.room",
                                                                    &*gallery.room);
    assert(galleryRules.interactions);
    assert(dialogueReferencesError(*galleryRules.interactions, *shipped.dialogues).empty());
    const auto firstRoom = RoomFiles::load(roomsDir / "first.room", sharedPlayerArt());
    const auto firstNone = DialogueFiles::loadOptionalForRoom(roomsDir / "first.room",
                                                              &*firstRoom.room);
    assert(firstNone.dialogues && firstNone.dialogues->empty() && firstNone.error.empty());
    assert(!DialogueFiles::load(directory / "missing.dlg").dialogues);

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
    assert(parsed.dialogues && parsed.error.empty());
    const DialogueChoice* key = parsed.dialogues->findNode("guarda")->findChoice("chave");
    assert(key->text == "Tenho a \"chave\"." && key->once && key->next == "liberado");
    assert(key->conditions.size() == 2 && key->effects.size() == 2);
    assert(parsed.dialogues->findNode("guarda")->lines[0].args[0] == "Alto la! #1");
    assert(parsed.dialogues->findNode("guarda")->findChoice("tchau")->effects.empty());

    const auto path = directory / "roundtrip.dlg";
    std::string error;
    assert(DialogueFiles::save(path, *parsed.dialogues, error, &room));
    const auto reloaded = DialogueFiles::load(path, &room);
    assert(reloaded.dialogues && reloaded.error.empty());
    assertSameDialogues(*parsed.dialogues, *reloaded.dialogues);
    const auto path2 = directory / "roundtrip2.dlg";
    assert(DialogueFiles::save(path2, *reloaded.dialogues, error, &room));
    std::ifstream first(path), second(path2);
    const std::string firstText((std::istreambuf_iterator<char>(first)), {});
    const std::string secondText((std::istreambuf_iterator<char>(second)), {});
    assert(!firstText.empty() && firstText == secondText);
    assert(!std::filesystem::exists(directory / "roundtrip.dlg.tmp"));

    // Invalid definitions are refused before touching the file.
    RoomDialogues broken = *parsed.dialogues;
    broken.nodes[0].choices[0].next = "nowhere";
    assert(!DialogueFiles::save(path, broken, error, &room));
    assert(error == "Destino de goto desconhecido: nowhere");
    assert(DialogueFiles::load(path, &room).dialogues);

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
        assert(!bad.dialogues);
        if (bad.error != testCase.expectedError) {
            SDL_Log("dialogue case: got '%s' expected '%s'", bad.error.c_str(), testCase.expectedError);
        }
        assert(bad.error == testCase.expectedError);
    }
}

// The choice menu is a ViewportUiConsumer: modal only while a choice is expected, driven by
// click/touch on a row, or up/down + confirm (keyboard, D-pad) and gamepad South.
// Free text reaches only the tools and gameplay UIs; the world drops it. Typing during a cutscene
// (with or without the tools layer, including the letters before "DEBUG") never replaces the line.
void testTypedTextNeverBecomesSpeech() {
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
    assert(game.inCutscene() && game.scene().floatingText().text() == "Linha");

    TranslatedInput typed;
    typed.textInput = "x";
    const InputRouter storeRouter; // no tools layer, as in a store build
    const InputRouteResult storeResult = storeRouter.routeTranslated(typed, game);
    assert(!storeResult.consumedByDebug && !storeResult.consumedByUi);
    assert(game.inCutscene() && game.scene().floatingText().text() == "Linha");

    DebugEditor editor;
    InputRouter toolsRouter;
    toolsRouter.setToolConsumer(&editor);
    typed.textInput = "DEBU"; // not yet the activation word: the editor lets it through
    assert(!toolsRouter.routeTranslated(typed, game).consumedByDebug);
    assert(!editor.active());
    assert(game.inCutscene() && game.scene().floatingText().text() == "Linha");

    // The line still ends after its reading time (2 s for "Linha"), not a 10 s echo.
    for (int i = 0; i < 50 && game.inCutscene(); ++i) game.tick(0.05F);
    assert(!game.inCutscene() && game.session().hasFlag("line_done"));
}

void testDialogueMenuInput() {
    Game game(RoomPresets::firstRoom());
    DialogueChoice a{.id = "a", .text = "Primeira", .conditions = {},
                     .effects = {InteractionEffect::make("set_flag", {"picked_a"})}};
    DialogueChoice b{.id = "b", .text = "Segunda", .conditions = {},
                     .effects = {InteractionEffect::make("set_flag", {"picked_b"})}};
    DialogueChoice c{.id = "c", .text = "Terceira", .conditions = {},
                     .effects = {InteractionEffect::make("set_flag", {"picked_c"})}};
    game.setDialogues(RoomDialogues{{DialogueNode{
            "menu", {InteractionEffect::make("say", {"Escolha"})}, {a, b, c}}}});
    DialogueMenu menu(game);
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
    assert(!press({900.0F, 600.0F}).consumedByUi);
    assert(game.scene().player().moving());
    for (int i = 0; i < 200 && game.scene().player().moving(); ++i) game.tick(0.05F);
    assert(!navigate(UiNavAction::down).consumedByUi);

    // Layout: rows stack inside the panel, at the bottom of the 1280x720 viewport.
    const SDL_FRect panel = DialogueMenu::panelRect(3);
    assert(panel.y + panel.h <= 720.0F && panel.x >= 0.0F && panel.x + panel.w <= 1280.0F);
    const SDL_FRect row1 = DialogueMenu::optionRect(1, 3);
    assert(DialogueMenu::optionAt(center(row1), 3) == 1u);
    assert(!DialogueMenu::optionAt({10.0F, 10.0F}, 3));

    // Click/touch on a row chooses it.
    assert(game.startDialogue("menu"));
    assert(game.awaitingDialogueChoice());
    const SDL_FPoint feet = game.scene().player().feet();
    assert(press({200.0F, 100.0F}).consumedByUi); // modal: outside the rows does nothing
    assert(game.awaitingDialogueChoice() && !game.scene().player().moving());
    assert(game.scene().player().feet().x == feet.x);
    assert(press(center(row1)).consumedByUi);
    assert(game.session().hasFlag("picked_b") && !game.dialogueActive());

    // Keyboard / D-pad: down wraps, confirm picks the highlighted row.
    assert(game.startDialogue("menu"));
    assert(menu.highlighted() == 0);
    assert(navigate(UiNavAction::up).consumedByUi);
    assert(menu.highlighted() == 2);
    assert(navigate(UiNavAction::down).consumedByUi);
    assert(menu.highlighted() == 0);
    assert(navigate(UiNavAction::cancel).consumedByUi && game.awaitingDialogueChoice());
    assert(navigate(UiNavAction::confirm).consumedByUi);
    assert(game.session().hasFlag("picked_a"));

    // Gamepad: D-pad highlights, South (a press at the unmoved virtual cursor) confirms it.
    assert(game.startDialogue("menu"));
    TranslatedInput hover;
    hover.pointerMoved = true;
    hover.viewportPoint = {200.0F, 100.0F};
    hover.insideViewport = true;
    (void)router.routeTranslated(hover, game);
    assert(navigate(UiNavAction::down).consumedByUi);
    assert(navigate(UiNavAction::down).consumedByUi);
    assert(press({200.0F, 100.0F}).consumedByUi);
    assert(game.session().hasFlag("picked_c"));

    // Hover follows the pointer and is consumed inside the panel.
    assert(game.startDialogue("menu"));
    hover.viewportPoint = center(DialogueMenu::optionRect(2, 3));
    assert(router.routeTranslated(hover, game).consumedByUi);
    assert(menu.highlighted() == 2 && !game.scene().pointerInside());

    // Draws without a window of its own (software renderer) while open.
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window* window = SDL_CreateWindow("Dialogue test", 1280, 720, SDL_WINDOW_HIDDEN);
    assert(window);
    Renderer renderer;
    assert(renderer.initialize(window));
    renderer.render(game, 1.0F, nullptr, &menu);
    SDL_Surface* pixels = SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr);
    assert(pixels);
    if (const char* capture = std::getenv("POINTCLICK_DIALOGUE_CAPTURE")) {
        assert(SDL_SaveBMP(pixels, capture));
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

void testActionCatalogRoundtrip(const std::filesystem::path& directory) {
    Room room = RoomPresets::firstRoom();
    room.scenery[1].animations["open"] = SpriteClip{.file = "red_open.png"};

    std::set<std::string_view> keywords;
    InteractionRule rule{.trigger = InteractionTrigger::interact("blue_box")};
    for (const ConditionDescriptor& condition : conditionCatalog()) {
        assert(!condition.description.empty() && condition.evaluate);
        assert(keywords.insert(condition.keyword).second);
        rule.conditions.push_back(
                InteractionCondition::make(condition.keyword, sampleArguments(condition)));
    }
    for (const EffectDescriptor& effect : effectCatalog()) {
        assert(!effect.description.empty() && effect.start);
        assert(keywords.insert(effect.keyword).second);
        rule.effects.push_back(InteractionEffect::make(effect.keyword, sampleArguments(effect)));
    }
    RoomInteractions authored{{rule}};
    assert(authored.validationError(&room).empty());

    const auto path = directory / "catalog.rules";
    std::string error;
    assert(InteractionFiles::save(path, authored, error, &room));
    const auto loaded = InteractionFiles::load(path, &room);
    assert(loaded.interactions && loaded.error.empty());
    const InteractionRule& back = loaded.interactions->rules.at(0);
    assert(back.conditions.size() == rule.conditions.size());
    assert(back.effects.size() == rule.effects.size());
    for (std::size_t i = 0; i < rule.conditions.size(); ++i) {
        assert(back.conditions[i].condition == rule.conditions[i].condition);
        assert(back.conditions[i].args == rule.conditions[i].args);
    }
    for (std::size_t i = 0; i < rule.effects.size(); ++i) {
        assert(back.effects[i].effect == rule.effects[i].effect);
        assert(back.effects[i].args == rule.effects[i].args);
    }

    // Typed validation applies to rules built in C++ as well as to parsed files.
    bool threw = false;
    try {
        (void)InteractionEffect::make("teleport", {"x"});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
    const auto invalid = [&](InteractionEffect effect) {
        RoomInteractions rules{{{.trigger = InteractionTrigger::interact("blue_box"),
                                 .effects = {std::move(effect)}}}};
        return rules.validationError(&room);
    };
    assert(invalid(InteractionEffect::make("set_solid", {"red_box", "2"}))
           == "Use set_solid <objeto> <0|1>");
    assert(invalid(InteractionEffect::make("set_solid", {"red_box"}))
           == "Use set_solid <objeto> <0|1>");
    assert(invalid(InteractionEffect::make("hide_object", {"fantasma"}))
           == "Objeto desconhecido na sala: fantasma");
    assert(invalid(InteractionEffect::make("play_animation", {"red_box", "danca"}))
           == "Animacao desconhecida 'danca' no objeto: red_box");
    assert(invalid(InteractionEffect::make("say", {""})) == "Texto vazio em say");
    assert(invalid(InteractionEffect::make("wait", {"1,5"})) == "Numero invalido: 1,5");
    assert(invalid(InteractionEffect::make("wait", {"-1"})) == "Numero invalido: -1");
    float seconds = 0.0F;
    assert(parseNumberArgument("0.25", seconds) && seconds == 0.25F);
    assert(parseNumberArgument("12", seconds) && seconds == 12.0F);
    assert(!parseNumberArgument("1.2.3", seconds) && !parseNumberArgument(".", seconds));
    assert(actionUsage("play_animation", findEffect("play_animation")->args)
           == "Use play_animation <objeto> <animacao>");
}
// The shipped layout: rooms inherit assets/actors/player.actor; RoomLibrary loads a room with
// its .rules and .dlg in one validated bundle for both the game and the editor.
void testRoomLibraryAndActors(const std::filesystem::path& directory) {
    RoomLibrary library;
    std::string error;
    assert(library.open(std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets/rooms", error));
    assert(library.playerArt().size() == 5);
    const auto first = library.loadBundle("first.room");
    assert(first.bundle && first.error.empty());
    assert(first.bundle->room.playerAnimations == library.playerArt()); // no copy in the room
    assert(!first.bundle->interactions.empty());
    const auto gallery = library.loadBundle("gallery.room");
    assert(gallery.bundle && gallery.bundle->dialogues.findNode("mesa"));
    assert(gallery.bundle->room.playerAnimations == library.playerArt());

    // Saving through the library keeps the shared art out of the room file.
    RoomLibrary scratch = openLibrary(directory);
    assert(scratch.playerArt().empty()); // no actors/ folder next to a temp directory
    assert(RoomFiles::save(directory / "art.room", first.bundle->room, error,
                           &library.playerArt()));
    std::ifstream saved(directory / "art.room");
    const std::string text((std::istreambuf_iterator<char>(saved)), {});
    assert(text.find("player.idle.") == std::string::npos);

    // The file name is the room's identity (change_room and SessionState must agree on it).
    Room declared = first.bundle->room;
    declared.id = "outra";
    assert(RoomFiles::save(directory / "trocada.room", declared, error));
    const auto mismatched = scratch.loadBundle("trocada.room");
    assert(!mismatched.bundle);
    assert(mismatched.error == "id = outra nao corresponde ao arquivo trocada.room (use id = trocada)");
    assert(!scratch.loadRoom("trocada.room").room);
    // A file without `id` is the room named after it, not Room's default "first".
    std::ifstream declaredFile(directory / "trocada.room");
    std::string withoutId;
    for (std::string line; std::getline(declaredFile, line);) {
        if (!line.starts_with("id = ")) withoutId += line + '\n';
    }
    declaredFile.close();
    std::ofstream(directory / "semid.room") << withoutId;
    const auto unnamed = scratch.loadBundle("semid.room");
    assert(unnamed.bundle && unnamed.bundle->room.id == "semid");
    // Saving under another name writes that name's id, so the copy loads back as its own room.
    assert(scratch.saveRoom("copia.room", declared, error));
    const auto copy = scratch.loadBundle("copia.room");
    assert(copy.bundle && copy.bundle->room.id == "copia");

    assert(ActorFiles::parse("").error == "Ator sem animacoes");
    assert(ActorFiles::parse("idle.file = a.png\nidle.file = b.png").error
           == "Linha 2: Chave repetida: idle.file");
    assert(ActorFiles::parse("idle.frames = 0").error
           == "Animacao idle: Contagem de quadros invalida");
    assert(ActorFiles::parse("idle.colour = 1").error
           == "Linha 1: Chave ou valor invalido: idle.colour");
}

// [spawn <id>] in .room (feet + facing) round-trips; change_room takes an optional spawn; the
// content check finds rooms and spawns that a change_room names but that do not exist.
void testRoomSpawnFilesAndTransitions(const std::filesystem::path& directory) {
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
    assert(hall.room && hall.error.empty());
    assert(hall.room->spawns.size() == 2);
    assert(hall.room->spawns[0] == (RoomSpawn{"door_left", {120.0F, 520.0F}, Facing::right}));
    assert(hall.room->spawns[1] == (RoomSpawn{"door_right", {1150.5F, 450.0F}, Facing::down}));

    std::string error;
    assert(RoomFiles::save(rooms / "copy.room", *hall.room, error, sharedPlayerArt()));
    std::ifstream savedFile(rooms / "copy.room");
    const std::string saved((std::istreambuf_iterator<char>(savedFile)), {});
    assert(saved.find("[spawn door_left]\nfeet = 120, 520\nfacing = right\n") != std::string::npos);
    const RoomLoadResult copy = RoomFiles::load(rooms / "copy.room", sharedPlayerArt());
    assert(copy.room && copy.room->spawns == hall.room->spawns);

    const auto errorOf = [&](const std::string& text) {
        write("bad.room", text);
        const RoomLoadResult result = RoomFiles::load(rooms / "bad.room", sharedPlayerArt());
        assert(!result.room);
        return result.error;
    };
    assert(errorOf("[spawn a]\nfacing = up\n") == "Spawn sem feet: a");
    assert(errorOf("[spawn a]\nfeet = 1, 2\nfacing = norte\n")
           == "Linha 3: Chave ou valor invalido: facing");
    assert(errorOf("[spawn a]\nfeet = 1\n") == "Linha 2: Chave ou valor invalido: feet");
    assert(errorOf("[spawn a]\nfeet = 1, 2\nbounds = 1, 2, 3, 4\n")
           == "Linha 3: Chave ou valor invalido: bounds");
    assert(errorOf("[spawn a]\nfeet = 1, 2\n[spawn a]\nfeet = 3, 4\n")
           == "Linha 4: Chave repetida: feet");
    assert(errorOf("[spawn a b]\nfeet = 1, 2\n") == "ID de spawn invalido ou repetido");
    assert(errorOf("[spawn a]\nfeet = 1300, 2\n") == "Spawn fora da sala: a");
    // A [spawn] in the middle does not swallow the room keys after a [room] header.
    write("mixed.room", "[spawn a]\nfeet = 1, 400\n[room]\nplayer.far_speed = 99\n");
    const RoomLoadResult mixed = RoomFiles::load(rooms / "mixed.room", sharedPlayerArt());
    assert(mixed.room && mixed.room->playerFarSpeed == 99.0F);

    // change_room <sala> [<spawn>]: one or two arguments, never three.
    const auto rulesOf = [](const std::string_view statement) {
        return InteractionFiles::parse("[interact x]\n" + std::string(statement) + "\n", nullptr);
    };
    const auto plain = rulesOf("change_room hall");
    assert(plain.interactions && plain.interactions->rules[0].effects[0].args.size() == 1);
    const auto spawned = rulesOf("change_room hall door_left # porta");
    assert(spawned.interactions);
    assert(spawned.interactions->rules[0].effects[0].args
           == (std::vector<std::string>{"hall", "door_left"}));
    assert(rulesOf("change_room hall door_left extra").error
           == "Linha 2: Use change_room <sala> [<spawn>]");
    assert(rulesOf("change_room").error == "Linha 2: Use change_room <sala> [<spawn>]");
    assert(rulesOf("change_room hall porta!").error == "Linha 2: Identificador invalido: porta!");
    const auto path = rooms / "spawned.rules";
    assert(InteractionFiles::save(path, *spawned.interactions, error, nullptr));
    std::ifstream rulesFile(path);
    const std::string rulesText((std::istreambuf_iterator<char>(rulesFile)), {});
    assert(rulesText.find("change_room hall door_left\n") != std::string::npos);

    // Content check over a bundle's transitions (rules and dialogue choices).
    RoomBundle bundle{*hall.room, *spawned.interactions, {}};
    assert(library.transitionsError(bundle).empty());
    bundle.interactions.rules[0].effects[0].args[1] = "window";
    assert(library.transitionsError(bundle) == "change_room hall: spawn desconhecido window");
    bundle.interactions = *plain.interactions;
    bundle.interactions.rules[0].effects[0].args[0] = "nowhere";
    assert(library.transitionsError(bundle).starts_with("change_room nowhere: "));
    bundle.interactions.rules.clear();
    DialogueChoice leave{.id = "sair", .text = "Sair"};
    leave.effects = {InteractionEffect::make("change_room", {"hall", "porta"})};
    bundle.dialogues.nodes = {DialogueNode{.id = "guarda", .lines = {}, .choices = {leave}}};
    assert(library.transitionsError(bundle) == "change_room hall: spawn desconhecido porta");

    // DEBUG `load <sala>.room [spawn]` enters through the spawn; an unknown one changes nothing.
    Game game(RoomPresets::firstRoom());
    DebugEditor editor;
    editor.setLibrary(library);
    assert(editor.onTextInput("DEBUG", game) && editor.active());
    command(editor, game, "load hall.room door_left");
    assert(editor.status() == "Sala carregada: hall.room em door_left");
    assert(game.scene().room().id == "hall" && game.scene().player().facing() == Facing::right);
    command(editor, game, "load first.room porta!");
    assert(editor.status() == "Use: load nome.room [spawn] (sem pastas)");
    command(editor, game, "load hall.room window");
    assert(editor.status() == "Spawn desconhecido na sala hall: window");
    assert(game.scene().room().id == "hall" && game.scene().player().facing() == Facing::right);
    command(editor, game, "save hall2.room extra");
    assert(editor.status() == "Use: save nome.room (sem pastas)");

    // The shipped rooms: every change_room points at a room and spawn that exist.
    RoomLibrary assets = openLibrary(std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets/rooms");
    for (const char* name : {"first.room", "gallery.room"}) {
        const RoomBundleResult shipped = assets.loadBundle(name);
        assert(shipped.bundle && assets.transitionsError(*shipped.bundle).empty());
    }
}
}

int main() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("pointclick-editor-tests-" + std::to_string(suffix));
    std::filesystem::create_directory(directory);
    testEditing(directory);
    testRoomFiles(directory);
    testDebugRendering();
    testMultiplatformInputAndRouting();
    testEditorPreservesSessionState(directory);
    testInteractionRulesFilesAndValidation(directory);
    testActionCatalogRoundtrip(directory);
    testDialogueFiles(directory);
    testDialogueMenuInput();
    testTypedTextNeverBecomesSpeech();
    testRoomLibraryAndActors(directory);
    testRoomSpawnFilesAndTransitions(directory);
    std::filesystem::remove_all(directory);
}
