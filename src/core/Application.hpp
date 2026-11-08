#pragma once

#include "content/RoomLibrary.hpp"
#include "content/SaveFiles.hpp"
#include "game/Autosave.hpp"
#include "game/Game.hpp"
#include "graphics/Renderer.hpp"
#include "input/InputRouter.hpp"
#include "input/InputSystem.hpp"
#include "ui/DialogueMenu.hpp"

#if POINTCLICK_TOOLS
#include "debug/DebugEditor.hpp"
#include "debug/DebugOverlay.hpp"
#endif

#include <memory>
#include <utility>
#include <vector>

struct SDL_Window;
union SDL_Event;

class Application {
public:
    explicit Application(std::filesystem::path roomDirectory = {});
    int run();

private:
    bool initialize();
    // Loads <id>.room with its companions; the first call creates the gameplay from it.
    // spawnId: a `[spawn]` of the room (empty = playerStart). Refused as a whole when unknown.
    bool loadRoomWithRules(std::string_view roomFilename, std::string_view spawnId = {});
    // Creates the gameplay from a loaded room. With a save, its progress is restored before
    // the room is applied and the feet go back where they were. Nothing changes on false.
    bool startGameplay(RoomBundle bundle, const SavedGame* save);
    // The save in the pref path, if there is a valid one; otherwise false and a new game.
    bool resumeSavedGame();
    // Routes the domain's notifications: room changes and the autosave's stable points.
    void processGameEvents();
    // Lifecycle edges that may write the autosave (once per trip to the background, on quit).
    void onEnterBackground();
    void onQuit();
    void writeAutosave(const SavedGame* save);
    // Mobile OSes may freeze the app as soon as the background event is queued; SDL asks for
    // that handling inside an event watch. Same thread as the loop on iOS/Android.
    static bool lifecycleWatch(void* userdata, SDL_Event* event);
    void mainLoop();
    void processEvents();
    void syncTextInputState();
    void shutdown();

    SDL_Window* window_ = nullptr;
    std::filesystem::path roomDirectory_;
    // Exists only once a room is loaded (the saved one or first.room): the binary carries no
    // placeholder room.
    struct Gameplay {
        Gameplay(Room room, SessionState progress) : game(std::move(room), std::move(progress)) {}
        Game game;
        DialogueMenu dialogueMenu{game};
    };

    RoomLibrary library_;
    std::unique_ptr<Gameplay> gameplay_;
    std::filesystem::path savePath_; // empty = no writable folder, saves disabled
    AutosavePolicy autosave_;
    bool autosaveErrorLogged_ = false;
    bool lifecycleWatchInstalled_ = false;
    std::vector<GameEvent> gameEvents_; // reused drain buffer
    Renderer renderer_;
#if POINTCLICK_TOOLS
    DebugEditor debugEditor_;
    DebugOverlay debugOverlay_{debugEditor_};
#endif
    InputSystem inputSystem_;
    InputRouter inputRouter_;
    float renderInterpolation_ = 1.0F;
    bool running_ = false;
    bool suspended_ = false;
    bool resetFrameClock_ = false;
};
