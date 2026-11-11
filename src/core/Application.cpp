#include "core/Application.hpp"
#include "content/TextFormat.hpp"
#include "game/Identifier.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <variant>

namespace {
// 60 Hz keeps click latency low and gives clips up to 60 fps a frame per tick; the renderer
// still interpolates positions, camera and animation time between ticks.
constexpr double kTicksPerSecond = 60.0;
// Without VSync, and while the window is not showing, this limiter paces the loop.
constexpr double kFramesPerSecond = 60.0;
// With VSync the display paces the frames; this only bounds a driver that ignores VSync.
constexpr double kVsyncSafetyFramesPerSecond = 240.0;
constexpr double kTickSeconds = 1.0 / kTicksPerSecond;
constexpr double kMaximumFrameTime = 0.25;
}

// The Game is created in initialize(), from the save or from first.room on disk.
Application::Application(CommandLineOptions options)
        : options_(std::move(options)), roomDirectory_(options_.roomsDirectory) {
#if POINTCLICK_TOOLS
    debugEditor_.setLibrary(library_);
    debugEditor_.setArtReloader([this] { renderer_.invalidateTextures(); });
    inputRouter_.setToolConsumer(&debugEditor_);
    renderer_.addOverlay(&debugOverlay_);
#endif
}

int Application::run() {
    if (!initialize()) {
        shutdown();
        return 1;
    }

    mainLoop();
    shutdown();
    return 0;
}

bool Application::loadRoomWithRules(const std::string_view roomFilename,
                                    const std::string_view spawnId) {
    RoomBundleResult loaded = library_.loadBundle(roomFilename);
    if (!loaded.bundle) {
        SDL_Log("Could not load room %.*s: %s", static_cast<int>(roomFilename.size()),
                roomFilename.data(), loaded.error.c_str());
        return false;
    }
    if (!gameplay_) return startGameplay(std::move(*loaded.bundle), nullptr, spawnId);
    std::string error;
    Game& game = gameplay_->game;
    if (!game.applyRoom(std::move(loaded.bundle->room), true, error, spawnId)
        || !game.setInteractions(std::move(loaded.bundle->interactions), error)
        || !game.setDialogues(std::move(loaded.bundle->dialogues), error)) {
        SDL_Log("Could not apply room: %s", error.c_str());
        return false;
    }
    return true;
}

bool Application::startGameplay(RoomBundle bundle, const SavedGame* save,
                                const std::string_view spawnId) {
    std::string error;
    const std::optional<RoomSpawn> entry = bundle.room.entryPoint(spawnId);
    if (!entry) {
        SDL_Log("Unknown spawn %.*s in room %s", static_cast<int>(spawnId.size()), spawnId.data(),
                bundle.room.id.c_str());
        return false;
    }
    SessionState progress;
    if (save && !progress.restore(save->session, error)) {
        SDL_Log("Could not restore the saved progress: %s", error.c_str());
        return false;
    }
    // Built aside and committed only when complete: a refused save leaves no half game behind.
    std::unique_ptr<Gameplay> gameplay;
    try {
        gameplay = std::make_unique<Gameplay>(std::move(bundle.room), std::move(progress),
                                              renderer_.textMetrics());
    } catch (const std::exception& exception) {
        SDL_Log("Could not apply room: %s", exception.what());
        return false;
    }
    Game& game = gameplay->game;
    if (!game.setInteractions(std::move(bundle.interactions), error)
        || !game.setDialogues(std::move(bundle.dialogues), error)) {
        SDL_Log("Could not apply room: %s", error.c_str());
        return false;
    }
    if (save) (void)game.placePlayerNear(save->feet, save->facing);
    else if (!spawnId.empty()) (void)game.placePlayerNear(entry->feet, entry->facing);
    gameplay_ = std::move(gameplay);
    inputRouter_.addUiConsumer(&gameplay_->dialogueMenu);
    return true;
}

bool Application::resumeSavedGame() {
    if (savePath_.empty()) return false;
    ResumeResult resumed = SaveFiles::resume(savePath_, library_);
    if (!resumed.warning.empty()) {
        SDL_Log("The saved room did not load, resuming the progress in %s: %s",
                resumed.bundle->room.id.c_str(), resumed.warning.c_str());
    }
    if (!resumed.save) {
        if (!resumed.error.empty()) {
            SDL_Log("Ignoring the save (kept as %s.bad), starting a new game: %s",
                    TextFormat::pathToUtf8(savePath_.filename()).c_str(), resumed.error.c_str());
        }
        return false;
    }
    if (startGameplay(std::move(*resumed.bundle), &*resumed.save)) return true;
    std::string renameError;
    if (!options_.noSave) (void)SaveFiles::quarantine(savePath_, renameError);
    SDL_Log("Ignoring the save (kept as %s.bad), starting a new game",
            TextFormat::pathToUtf8(savePath_.filename()).c_str());
    return false;
}

void Application::processGameEvents() {
    gameplay_->game.drainEvents(gameEvents_);
    const RoomChangeRequested* nextRoom = nullptr;
    bool stablePoint = false;
    bool enteredRoom = false;
    for (const GameEvent& event : gameEvents_) {
        if (const auto* change = std::get_if<RoomChangeRequested>(&event)) {
            nextRoom = change; // several in one drain: the last request wins
        } else if (std::holds_alternative<RoomEntered>(event)) {
            enteredRoom = true;
        } else if (std::holds_alternative<CutsceneEnded>(event)) {
            stablePoint = true;
        }
        // LineSpoken and CutsceneStarted have no shell consumer yet (audio, fades, subtitles).
    }
    // A pending room change is not a stable point yet: its RoomEntered will be.
    const bool loadingRoom = nextRoom && validIdentifier(nextRoom->roomId)
                             && (nextRoom->spawnId.empty() || validIdentifier(nextRoom->spawnId));
    if (nextRoom && !loadingRoom) {
        SDL_Log("Invalid room transition target: %s %s", nextRoom->roomId.c_str(),
                nextRoom->spawnId.c_str());
    }
    if (loadingRoom && loadRoomWithRules(nextRoom->roomId + ".room", nextRoom->spawnId)) return;
    // Entering a room also writes the file, so a crash costs at most the current room.
    if (enteredRoom) writeAutosave(autosave_.checkpoint(gameplay_->game));
    else if (stablePoint) autosave_.capture(gameplay_->game);
}

void Application::onEnterBackground() {
    suspended_ = true;
    if (gameplay_) writeAutosave(autosave_.enterBackground(gameplay_->game));
}

void Application::onQuit() {
    running_ = false;
    if (gameplay_) writeAutosave(autosave_.quit(gameplay_->game));
}

void Application::writeAutosave(const SavedGame* save) {
    if (!save || savePath_.empty() || options_.noSave) return;
    std::string error;
    if (SaveFiles::save(savePath_, *save, error)) {
        autosave_.markWritten();
        autosaveErrorLogged_ = false;
    } else if (!autosaveErrorLogged_) {
        SDL_Log("Could not write the autosave: %s", error.c_str());
        autosaveErrorLogged_ = true;
    }
}

bool Application::lifecycleWatch(void* userdata, SDL_Event* event) {
    auto* application = static_cast<Application*>(userdata);
    // Off the loop's thread the Game may be mid-tick; the polled copy of the event covers it.
    if (!SDL_IsMainThread() || !application->gameplay_) return true;
    switch (event->type) {
    case SDL_EVENT_WILL_ENTER_BACKGROUND:
    case SDL_EVENT_DID_ENTER_BACKGROUND:
        application->onEnterBackground();
        break;
    case SDL_EVENT_TERMINATING:
        application->onQuit();
        break;
    default:
        break;
    }
    return true;
}

bool Application::initialize() {
    // Audio is initialized by its own module when there is one: a machine without an audio
    // driver must still run the game.
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Could not initialize SDL: %s", SDL_GetError());
        return false;
    }
    inputSystem_.initialize();

    window_ = SDL_CreateWindow(
            "Point & Click",
            1280,
            720,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );

    if (window_ == nullptr) {
        SDL_Log("Could not create the window: %s", SDL_GetError());
        return false;
    }

    if (!renderer_.initialize(window_)) {
        return false;
    }

    if (roomDirectory_.empty()) {
        const char* base = SDL_GetBasePath();
        if (!base) { SDL_Log("Could not locate assets: %s", SDL_GetError()); return false; }
        // SDL owns this cached UTF-8 path; on Android it is "./" for APK assets.
        const std::u8string basePath(base, base + SDL_strlen(base));
        roomDirectory_ = std::filesystem::path(basePath) / "assets" / "rooms";
    }
    if (std::string error; !library_.open(roomDirectory_, error)) {
        SDL_Log("Could not load shared actors: %s", error.c_str());
        return false;
    }
    renderer_.setSpriteRoot(library_.spritesDirectory());
    // A missing font is logged and drawn with the debug font: the game still starts.
    (void)renderer_.setFontRoot(library_.fontsDirectory());
    if (std::string error; (savePath_ = SaveFiles::defaultSlot(error)).empty()) {
        SDL_Log("No writable folder for saves, playing without them: %s", error.c_str());
    }
    if (options_.noSave) SDL_Log("--no-save: the autosave will not be written");
    const std::string startRoom = options_.startRoom.empty()
                                  ? std::string(RoomLibrary::startRoomFile)
                                  : options_.startRoom + ".room";
    if (!(options_.resumesSave() && resumeSavedGame())
        && !loadRoomWithRules(startRoom, options_.startSpawn)) {
        return false;
    }
    autosave_.capture(gameplay_->game);
    lifecycleWatchInstalled_ = SDL_AddEventWatch(&Application::lifecycleWatch, this);

    // Only the tools take free text. Avoid opening the on-screen virtual keyboard on
    // mobile/console boot; store builds never start text input.
#if POINTCLICK_TOOLS
    if (!SDL_HasScreenKeyboardSupport() && !SDL_StartTextInput(window_)) {
        SDL_Log("Could not start text input: %s", SDL_GetError());
        return false;
    }
#endif

    running_ = true;
    return true;
}

void Application::mainLoop() {
    using Clock = std::chrono::steady_clock;

    const auto stepFor = [](const double framesPerSecond) {
        return std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>(1.0 / framesPerSecond));
    };
    const auto limiterStep = stepFor(kFramesPerSecond);
    const auto vsyncStep = stepFor(kVsyncSafetyFramesPerSecond);

    auto previousTime = Clock::now();
    auto nextRenderTime = previousTime;
    double tickAccumulator = 0.0;
    Game& game = gameplay_->game; // initialize() succeeded, so first.room is loaded

    while (running_) {
        processEvents();
        if (!running_) {
            break;
        }
        processGameEvents();
        if (suspended_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }
        if (resetFrameClock_) {
            previousTime = Clock::now();
            nextRenderTime = previousTime;
            tickAccumulator = 0.0;
            resetFrameClock_ = false;
        }

        const auto currentTime = Clock::now();
        const double elapsedSeconds = std::chrono::duration<double>(
                currentTime - previousTime
        ).count();
        previousTime = currentTime;

        const double frameSeconds = std::min(elapsedSeconds, kMaximumFrameTime);
        tickAccumulator += frameSeconds;

        while (tickAccumulator >= kTickSeconds) {
            game.tick(static_cast<float>(kTickSeconds));
            processGameEvents();
            tickAccumulator -= kTickSeconds;
        }

        renderInterpolation_ = static_cast<float>(tickAccumulator / kTickSeconds);
        inputRouter_.update(static_cast<float>(frameSeconds), inputSystem_, game,
                            renderInterpolation_);

        // Minimized or fully covered: keep simulating, draw nothing (presenting would not wait
        // for the display there, so the limiter paces the loop).
        const bool showing = (SDL_GetWindowFlags(window_)
                              & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_OCCLUDED | SDL_WINDOW_HIDDEN))
                             == 0;
        const auto renderStep = showing && renderer_.vsyncActive() ? vsyncStep : limiterStep;
        if (currentTime >= nextRenderTime) {
            if (showing) {
                renderer_.render(game, renderInterpolation_, &inputSystem_,
                                 &gameplay_->dialogueMenu);
            }

            nextRenderTime += renderStep;
            if (nextRenderTime < currentTime) {
                nextRenderTime = currentTime + renderStep;
            }
        } else {
            std::this_thread::sleep_until(nextRenderTime);
        }
    }
}

void Application::processEvents() {
    const ViewportCoordinateMapper mapper = renderer_.viewportMapper();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        const InputRouteResult result = inputRouter_.handleEvent(
                event, inputSystem_, gameplay_->game, mapper, renderInterpolation_);
        if (result.quitRequested) {
            onQuit();
        }
        if (result.enteredBackground) {
            onEnterBackground(); // WILL and DID (and the watch) write once per trip
        }
        if (result.enteredForeground) {
            suspended_ = false;
            autosave_.enterForeground();
            resetFrameClock_ = true;
        }
        if (result.invalidateGraphics) {
            renderer_.invalidateTextures();
        }
    }
    syncTextInputState();
}

void Application::syncTextInputState() {
    if (window_ == nullptr || !SDL_HasScreenKeyboardSupport()) {
        return;
    }
    // Only the tools take free text today; store builds never open the on-screen keyboard.
#if POINTCLICK_TOOLS
    const bool wantTextInput = debugEditor_.panelVisible()
                               && inputSystem_.activeDevice() != InputDevice::gamepad;
#else
    const bool wantTextInput = false;
#endif
    if (wantTextInput && !SDL_TextInputActive(window_)) {
        SDL_StartTextInput(window_);
    } else if (!wantTextInput && SDL_TextInputActive(window_)) {
        SDL_StopTextInput(window_);
    }
}

void Application::shutdown() {
    if (lifecycleWatchInstalled_) {
        SDL_RemoveEventWatch(&Application::lifecycleWatch, this);
        lifecycleWatchInstalled_ = false;
    }
    if (window_ != nullptr && SDL_TextInputActive(window_)) {
        SDL_StopTextInput(window_);
    }

    inputSystem_.shutdown();
    renderer_.shutdown();

    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
}
