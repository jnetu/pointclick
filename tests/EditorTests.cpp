#include "content/RoomFiles.hpp"
#include "content/RoomPresets.hpp"
#include "game/RoomParameters.hpp"
#include "debug/DebugEditor.hpp"
#include "game/Game.hpp"
#include "graphics/Renderer.hpp"
#include "input/InputRouter.hpp"
#include "input/InputSystem.hpp"

#include <SDL3/SDL.h>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>

namespace {
void command(DebugEditor& editor, Game& game, const std::string_view text) {
    assert(editor.onTextInput(text, game));
    assert(editor.onKey(DebugKey::submit, game));
}

void testEditing(const std::filesystem::path& directory) {
    Game game(RoomPresets::firstRoom());
    DebugEditor editor;
    editor.setRoomDirectory(directory);
    assert(!editor.onKey(DebugKey::increase, game));
    assert(!editor.onTextInput("DE", game));
    assert(!editor.active());
    assert(editor.onTextInput("bug", game));
    assert(editor.active());
    editor.onKey(DebugKey::increase, game);
    assert(game.room().playerFarSpeed == 120);
    command(editor, game, "set player.near_speed 400");
    assert(game.room().playerNearSpeed == 400);

    game.onClick({700, 400});
    const auto before = game.player().destination();
    command(editor, game, "set perspective.near_y 100");
    assert(game.room().nearDepthY == 720);
    assert(game.player().moving()); // Rejected changes preserve the whole simulation.
    assert(game.player().destination().x == before.x);
    command(editor, game, "set player.width -1");
    assert(game.room().playerBaseSize.x == 48);
    command(editor, game, "set player.width nan");
    assert(game.room().playerBaseSize.x == 48);

    command(editor, game, "save authored.room");
    command(editor, game, "set player.near_speed 500");
    command(editor, game, "undo");
    assert(game.room().playerNearSpeed == 400);
    command(editor, game, "load authored.room");
    assert(game.room().playerFarSpeed == 120);
    assert(!game.player().moving());
    command(editor, game, "save ../escape.room");
    assert(editor.status().find("sem pastas") != std::string::npos);

    command(editor, game, "teleport 860 575");
    assert(game.player().feet().x == 640); // Cannot place feet in the red base.
    command(editor, game, "move red_box 620 240");
    assert(game.player().feet().x != 640 || game.player().feet().y != 360);
    assert(game.room().scenery[1].bounds.x == 620);
    command(editor, game, "move red_box 2000 240");
    assert(game.room().scenery[1].bounds.x == 620);
    command(editor, game, "set room.width 1000");
    assert(game.room().width == 1280);
    command(editor, game, "set room.height 600");
    assert(game.room().height == 720);
    command(editor, game, "set room.width 2560");
    assert(game.room().width == 2560);
    command(editor, game, "move red_box 2000 240");
    assert(game.room().scenery[1].bounds.x == 2000);
    command(editor, game, "set room.width 1280");
    assert(game.room().width == 2560); // Cannot shrink past an object at x = 2000.
    command(editor, game, "reset");
    assert(game.room().width == 1280);
    assert(game.room().playerFarSpeed == 110);
    assert(game.room().scenery[1].bounds.x == 810);

    editor.onKey(DebugKey::togglePanel, game);
    assert(editor.active() && !editor.panelVisible());
    editor.onKey(DebugKey::increase, game);
    assert(game.room().playerFarSpeed == 110);
    assert(editor.onTextInput("set player.far_speed 500", game));
    assert(editor.command().empty());
    editor.onKey(DebugKey::togglePanel, game);
    assert(editor.panelVisible());

    editor.onKey(DebugKey::close, game);
    assert(!editor.active());
    assert(!editor.onKey(DebugKey::increase, game));
    assert(game.room().playerFarSpeed == 110);

    Room blocked = game.room();
    blocked.scenery = {{{0, 0, 1280, 720}, {255, 0, 0, 255}, SceneObjectType::solid,
                       {0, 0, 1280, 720}, "wall"}};
    std::string error;
    assert(!game.applyRoom(blocked, false, error));
    assert(game.room().scenery.size() == 2);
}

void testRoomFiles(const std::filesystem::path& directory) {
    const auto source = std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets/rooms/first.room";
    const auto loaded = RoomFiles::load(source);
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
    const auto commented = RoomFiles::load(commentedPath);
    assert(commented.room && commented.error.empty());
    assert(commented.room->id == "commented");
    assert(commented.room->playerFarSpeed == 123.5F);

    const auto emptyPath = directory / "empty.room";
    { std::ofstream out(emptyPath, std::ios::binary); }
    const auto empty = RoomFiles::load(emptyPath);
    assert(empty.room && empty.error.empty());
    assert(empty.room->id == RoomPresets::defaults().id);

    const auto nulPath = directory / "nul.room";
    {
        std::ofstream out(nulPath, std::ios::binary);
        out << "[room]\nid = nul_room\n";
        out.put('\0');
        out << '\n';
    }
    const auto nul = RoomFiles::load(nulPath);
    assert(!nul.room && nul.error == "Linha 3: Esperado chave = valor");

    const auto gallery = RoomFiles::load(source.parent_path() / "gallery.room");
    assert(gallery.room);
    Game other(*gallery.room);
    assert(other.room().id == "gallery");
    other.onClick({400, 460});
    assert(other.player().moving());
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
    editor.onTextInput("DEBUG", game);
    renderer.render(game, 1.0F, &editor);
    SDL_Surface* pixels = SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr);
    assert(pixels);
    if (const char* capture = std::getenv("POINTCLICK_DEBUG_CAPTURE")) assert(SDL_SaveBMP(pixels, capture));
    SDL_DestroySurface(pixels);
    // Resizing preserves the logical center used by the mouse and editor.
    assert(SDL_SetWindowSize(window, 1000, 1000));
    SDL_PumpEvents();
    renderer.render(game, 1.0F, &editor);
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
    assert(std::abs(game.camera().position().x) < 0.01F);

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
                                      game.camera().position()));
        assert(std::abs(vpCenter.x - 640.0F) < 0.5F && std::abs(vpCenter.y - 360.0F) < 0.5F);
        assert(std::abs(worldCenter.x - 640.0F) < 0.5F && std::abs(worldCenter.y - 360.0F) < 0.5F);

        SDL_FPoint leftEdge{};
        SDL_FPoint rightEdge{};
        assert(renderer.windowToWorld(presentation.x + 0.5F, res.h * 0.5F, leftEdge,
                                      game.camera().position()));
        assert(renderer.windowToWorld(presentation.x + presentation.w - 0.5F, res.h * 0.5F,
                                      rightEdge, game.camera().position()));
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
    assert(std::abs(game.camera().position().x - 910.0F) < 0.01F);
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
                                  game.camera().position()));
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
    assert(std::abs(game.camera().position().x - 910.0F) < 0.01F);
    renderer.render(game, 1.0F, nullptr);

    DebugEditor editor;
    InputSystem input;
    InputRouter router;
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
    auto routeResult = router.handleEvent(panelClick, input, editor, game, mapper);
    assert(routeResult.consumedByDebug);
    assert(!game.player().moving());

    // Clicking outside the panel at Viewport (300, 520) -> World (1210, 520) walks the player.
    SDL_Event outsidePanelClick = panelClick;
    outsidePanelClick.button.x = 440.0F + 300.0F * 2.0F;
    outsidePanelClick.button.y = 520.0F * 2.0F;
    routeResult = router.handleEvent(outsidePanelClick, input, editor, game, mapper);
    assert(!routeResult.consumedByDebug);
    assert(game.player().moving());
    assert(std::abs(game.player().destination().x - 1210.0F) < 0.5F);
    assert(std::abs(game.player().destination().y - 520.0F) < 0.5F);

    std::string teleportError;
    assert(game.teleportPlayer({1550.0F, 520.0F}, teleportError));

    // Hiding the panel with TAB allows clicking at Viewport (900, 500) -> World (1810, 500).
    SDL_Event tabKey{};
    tabKey.type = SDL_EVENT_KEY_DOWN;
    tabKey.key.key = SDLK_TAB;
    tabKey.key.down = true;
    assert(router.handleEvent(tabKey, input, editor, game, mapper).consumedByDebug);
    assert(editor.active() && !editor.panelVisible());
    assert(!editor.consumesViewportPoint({900.0F, 500.0F}));

    routeResult = router.handleEvent(panelClick, input, editor, game, mapper);
    assert(!routeResult.consumedByDebug);
    assert(game.player().moving());
    assert(std::abs(game.player().destination().x - 1810.0F) < 0.5F);
    assert(std::abs(game.player().destination().y - 500.0F) < 0.5F);

    assert(game.teleportPlayer({1550.0F, 520.0F}, teleportError));

    // Closing the editor with ESC also leaves the panel area unblocked.
    SDL_Event escKey{};
    escKey.type = SDL_EVENT_KEY_DOWN;
    escKey.key.key = SDLK_ESCAPE;
    escKey.key.down = true;
    assert(router.handleEvent(escKey, input, editor, game, mapper).consumedByDebug);
    assert(!editor.active());
    routeResult = router.handleEvent(panelClick, input, editor, game, mapper);
    assert(!routeResult.consumedByDebug);
    assert(game.player().moving());
    assert(std::abs(game.player().destination().x - 1810.0F) < 0.5F);

    assert(game.teleportPlayer({1550.0F, 520.0F}, teleportError));

    // 2. Touch translation + letterbox/pillarbox rejection + SDL_TOUCH_MOUSEID filtering.
    // Touch inside left Ultrawide pillarbox bar (normalized x = 0.02 -> window x = 68.8 < 440).
    SDL_Event pillarboxTouch{};
    pillarboxTouch.type = SDL_EVENT_FINGER_DOWN;
    pillarboxTouch.tfinger.touchID = 10;
    pillarboxTouch.tfinger.fingerID = 1;
    pillarboxTouch.tfinger.x = 0.02F;
    pillarboxTouch.tfinger.y = 0.75F;
    (void)router.handleEvent(pillarboxTouch, input, editor, game, mapper);
    assert(!game.player().moving());
    assert(!game.pointerInside());

    SDL_Event pillarboxTouchUp = pillarboxTouch;
    pillarboxTouchUp.type = SDL_EVENT_FINGER_UP;
    (void)router.handleEvent(pillarboxTouchUp, input, editor, game, mapper);

    // Touch at window center (normalized 0.5, 0.75) -> Viewport (640, 540) -> World (1550, 540).
    // Let's tap slightly right of center: window x = 440 + 800 * 2 = 2040 -> nx = 2040 / 3440.
    SDL_Event worldTouch{};
    worldTouch.type = SDL_EVENT_FINGER_DOWN;
    worldTouch.tfinger.touchID = 10;
    worldTouch.tfinger.fingerID = 2;
    worldTouch.tfinger.x = (440.0F + 800.0F * 2.0F) / 3440.0F;
    worldTouch.tfinger.y = 540.0F / 720.0F;
    (void)router.handleEvent(worldTouch, input, editor, game, mapper);
    assert(input.activeDevice() == InputDevice::touch);
    assert(game.player().moving());
    assert(std::abs(game.player().destination().x - 1710.0F) < 0.5F); // 800 + 910
    assert(std::abs(game.player().destination().y - 540.0F) < 0.5F);

    // Synthetic mouse motion & click emitted by touch must be ignored (no double-click / retarget).
    SDL_Event syntheticMouseClick{};
    syntheticMouseClick.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    syntheticMouseClick.button.which = SDL_TOUCH_MOUSEID;
    syntheticMouseClick.button.button = SDL_BUTTON_LEFT;
    syntheticMouseClick.button.down = true;
    syntheticMouseClick.button.x = 440.0F + 200.0F * 2.0F;
    syntheticMouseClick.button.y = 600.0F * 2.0F;
    (void)router.handleEvent(syntheticMouseClick, input, editor, game, mapper);
    assert(input.activeDevice() == InputDevice::touch);
    assert(std::abs(game.player().destination().x - 1710.0F) < 0.5F);

    SDL_Event syntheticMouseTouch{};
    syntheticMouseTouch.type = SDL_EVENT_FINGER_DOWN;
    syntheticMouseTouch.tfinger.touchID = SDL_MOUSE_TOUCHID;
    syntheticMouseTouch.tfinger.fingerID = 3;
    syntheticMouseTouch.tfinger.x = 0.3F;
    syntheticMouseTouch.tfinger.y = 0.8F;
    (void)router.handleEvent(syntheticMouseTouch, input, editor, game, mapper);
    assert(std::abs(game.player().destination().x - 1710.0F) < 0.5F);

    SDL_Event worldTouchUp = worldTouch;
    worldTouchUp.type = SDL_EVENT_FINGER_UP;
    (void)router.handleEvent(worldTouchUp, input, editor, game, mapper);

    assert(game.teleportPlayer({1550.0F, 520.0F}, teleportError));

    // 3. Gamepad hotplug, deadzone, virtual cursor movement, and scrolled world click.
    SDL_Event padAdded{};
    padAdded.type = SDL_EVENT_GAMEPAD_ADDED;
    padAdded.gdevice.which = 77;
    (void)router.handleEvent(padAdded, input, editor, game, mapper);
    assert(input.gamepadCount() == 1);

    input.setCursorViewport({640.0F, 360.0F});
    input.setCursorSpeed(400.0F);

    // Sub-deadzone stick deflection must produce zero movement.
    SDL_Event smallTilt{};
    smallTilt.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    smallTilt.gaxis.which = 77;
    smallTilt.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTX;
    smallTilt.gaxis.value = 3000; // ~0.09 < 0.24 deadzone
    (void)router.handleEvent(smallTilt, input, editor, game, mapper);
    assert(input.moveAxis().x == 0.0F && input.moveAxis().y == 0.0F);
    router.update(0.5F, input, editor, game);
    assert(std::abs(input.cursorViewport().x - 640.0F) < 0.01F);

    // Full stick deflection right & down moves the virtual cursor smoothly in viewport space.
    SDL_Event stickRight = smallTilt;
    stickRight.gaxis.value = 32767;
    (void)router.handleEvent(stickRight, input, editor, game, mapper);
    assert(std::abs(input.moveAxis().x - 1.0F) < 0.01F);

    SDL_Event stickDown = smallTilt;
    stickDown.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTY;
    stickDown.gaxis.value = 32767;
    (void)router.handleEvent(stickDown, input, editor, game, mapper);
    assert(std::hypot(input.moveAxis().x, input.moveAxis().y) <= 1.001F);

    // Zero Y, move purely right by 200px (0.5s * 400px/s) and down by 100px: cursor -> (840, 460).
    stickDown.gaxis.value = 0;
    (void)router.handleEvent(stickDown, input, editor, game, mapper);
    router.update(0.5F, input, editor, game);
    assert(std::abs(input.cursorViewport().x - 840.0F) < 0.1F);

    stickRight.gaxis.value = 0;
    (void)router.handleEvent(stickRight, input, editor, game, mapper);
    stickDown.gaxis.value = 32767;
    (void)router.handleEvent(stickDown, input, editor, game, mapper);
    router.update(0.25F, input, editor, game);
    assert(std::abs(input.cursorViewport().y - 460.0F) < 0.1F);

    // Primary gamepad button (SOUTH) clicks at virtual cursor (840, 460) -> World (1750, 460).
    SDL_Event southPress{};
    southPress.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    southPress.gbutton.which = 77;
    southPress.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
    southPress.gbutton.down = true;
    (void)router.handleEvent(southPress, input, editor, game, mapper);
    assert(input.activeDevice() == InputDevice::gamepad);
    assert(game.player().moving());
    assert(std::abs(game.player().destination().x - 1750.0F) < 0.5F); // 840 + 910
    assert(std::abs(game.player().destination().y - 460.0F) < 0.5F);

    // Virtual cursor clamps cleanly at viewport boundaries [0..1279] x [0..719].
    router.update(10.0F, input, editor, game);
    assert(Renderer::containsViewportPoint(input.cursorViewport()));
    assert(std::abs(input.cursorViewport().y - 719.0F) < 0.01F);

    // Unplugging the gamepad resets held stick axes to prevent drift.
    SDL_Event padRemoved{};
    padRemoved.type = SDL_EVENT_GAMEPAD_REMOVED;
    padRemoved.gdevice.which = 77;
    (void)router.handleEvent(padRemoved, input, editor, game, mapper);
    assert(input.gamepadCount() == 0);
    assert(input.moveAxis().x == 0.0F && input.moveAxis().y == 0.0F);

    // 4. DebugEditor open/navigate/adjust via Keyboard (F1 + arrows) and Gamepad (BACK + D-Pad/Shoulders).
    SDL_Event f1Key{};
    f1Key.type = SDL_EVENT_KEY_DOWN;
    f1Key.key.key = SDLK_F1;
    f1Key.key.down = true;
    assert(router.handleEvent(f1Key, input, editor, game, mapper).consumedByDebug);
    assert(editor.active() && editor.panelVisible());

    SDL_Event padBack{};
    padBack.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    padBack.gbutton.which = 77;
    padBack.gbutton.button = SDL_GAMEPAD_BUTTON_BACK;
    padBack.gbutton.down = true;
    assert(router.handleEvent(padBack, input, editor, game, mapper).consumedByDebug);
    assert(!editor.active()); // Toggled closed by gamepad BACK.
    assert(router.handleEvent(padBack, input, editor, game, mapper).consumedByDebug);
    assert(editor.active() && editor.panelVisible()); // Re-opened without typing "DEBUG"!

    const float initialFarSpeed = game.room().playerFarSpeed;
    assert(editor.selected() == 0);

    SDL_Event dpadRight = padBack;
    dpadRight.gbutton.button = SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    assert(router.handleEvent(dpadRight, input, editor, game, mapper).consumedByDebug);
    assert(game.room().playerFarSpeed > initialFarSpeed);

    SDL_Event leftShoulder = padBack;
    leftShoulder.gbutton.button = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    assert(router.handleEvent(leftShoulder, input, editor, game, mapper).consumedByDebug);
    assert(std::abs(game.room().playerFarSpeed - initialFarSpeed) < 0.01F);

    SDL_Event dpadDown = padBack;
    dpadDown.gbutton.button = SDL_GAMEPAD_BUTTON_DPAD_DOWN;
    assert(router.handleEvent(dpadDown, input, editor, game, mapper).consumedByDebug);
    assert(editor.selected() == 1);

    SDL_Event keyUp{};
    keyUp.type = SDL_EVENT_KEY_DOWN;
    keyUp.key.key = SDLK_UP;
    keyUp.key.down = true;
    assert(router.handleEvent(keyUp, input, editor, game, mapper).consumedByDebug);
    assert(editor.selected() == 0);

    SDL_Event padWest = padBack;
    padWest.gbutton.button = SDL_GAMEPAD_BUTTON_WEST;
    assert(router.handleEvent(padWest, input, editor, game, mapper).consumedByDebug);
    assert(editor.active() && !editor.panelVisible());
    assert(router.handleEvent(padWest, input, editor, game, mapper).consumedByDebug);
    assert(editor.panelVisible());

    SDL_Event padEast = padBack;
    padEast.gbutton.button = SDL_GAMEPAD_BUTTON_EAST;
    assert(router.handleEvent(padEast, input, editor, game, mapper).consumedByDebug);
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
    routeResult = router.handleEvent(modalClick, input, editor, game, mapper);
    assert(routeResult.consumedByUi);
    assert(!game.player().moving());

    routeResult = router.handleEvent(dpadDown, input, editor, game, mapper);
    assert(routeResult.consumedByUi && modalUi.navCount == 1);
    router.clearUiConsumers();

    // Lifecycle signals: background clears transient axes, device reset signals invalidation.
    stickRight.gaxis.value = 32767;
    (void)router.handleEvent(stickRight, input, editor, game, mapper);
    assert(input.moveAxis().x > 0.5F);

    SDL_Event bgEvent{};
    bgEvent.type = SDL_EVENT_WILL_ENTER_BACKGROUND;
    routeResult = router.handleEvent(bgEvent, input, editor, game, mapper);
    assert(routeResult.enteredBackground);
    assert(input.moveAxis().x == 0.0F && input.moveAxis().y == 0.0F);

    SDL_Event fgEvent{};
    fgEvent.type = SDL_EVENT_DID_ENTER_FOREGROUND;
    assert(router.handleEvent(fgEvent, input, editor, game, mapper).enteredForeground);

    SDL_Event resetEvent{};
    resetEvent.type = SDL_EVENT_RENDER_DEVICE_RESET;
    assert(router.handleEvent(resetEvent, input, editor, game, mapper).invalidateGraphics);
    renderer.invalidateTextures();

    SDL_Event quitEvent{};
    quitEvent.type = SDL_EVENT_QUIT;
    assert(router.handleEvent(quitEvent, input, editor, game, mapper).quitRequested);

    renderer.shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
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
    std::filesystem::remove_all(directory);
}
