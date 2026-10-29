#include "content/RoomFiles.hpp"
#include "content/RoomPresets.hpp"
#include "game/RoomParameters.hpp"
#include "debug/DebugEditor.hpp"
#include "game/Game.hpp"
#include "graphics/Renderer.hpp"

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
}

int main() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("pointclick-editor-tests-" + std::to_string(suffix));
    std::filesystem::create_directory(directory);
    testEditing(directory);
    testRoomFiles(directory);
    testDebugRendering();
    std::filesystem::remove_all(directory);
}
