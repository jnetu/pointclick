#include "support/WalkthroughDriver.hpp"

#include "content/SaveFiles.hpp"
#include "game/Identifier.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>
#include <variant>

namespace {
std::string number(const float value) {
    std::ostringstream text;
    text << value;
    return text.str();
}

std::string text(const std::string& value) {
    return "\"" + value + "\"";
}
std::string text(const std::string_view value) {
    return "\"" + std::string(value) + "\"";
}
std::string text(const int value) {
    return std::to_string(value);
}
std::string text(const bool value) {
    return value ? "sim" : "nao";
}
std::string text(const Facing value) {
    return std::string(facingName(value));
}
std::string text(const std::vector<std::string>& values) {
    std::string result = "[";
    for (std::size_t i = 0; i < values.size(); ++i) {
        result += (i == 0 ? "" : ", ") + text(values[i]);
    }
    return result + "]";
}

std::vector<std::string> toStrings(const std::initializer_list<std::string_view> values) {
    return {values.begin(), values.end()};
}

std::filesystem::path shippedRooms() {
    return std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets" / "rooms";
}
} // namespace

WalkthroughDriver::WalkthroughDriver(const std::string_view room, const std::string_view spawn)
        : WalkthroughDriver(room, spawn, shippedRooms()) {}

WalkthroughDriver::WalkthroughDriver(const std::string_view room, const std::string_view spawn,
                                     std::filesystem::path roomsDirectory) {
    begin("start new game in " + std::string(room)
          + (spawn.empty() ? "" : " " + std::string(spawn)));
    std::string error;
    if (!library_.open(std::move(roomsDirectory), error)) fail("pasta de salas: " + error);
    RoomBundleResult loaded = library_.loadBundle(std::string(room) + ".room");
    if (!loaded.bundle) fail(loaded.errorFile + ": " + loaded.error);
    startGame(std::move(*loaded.bundle), nullptr, spawn);
}

// What Application::startGameplay does, minus the window: progress first, then the room, its
// rules and dialogues, then the spawn (or the saved feet).
void WalkthroughDriver::startGame(RoomBundle bundle, const SavedGame* save,
                                  const std::string_view spawnId) {
    const std::optional<RoomSpawn> entry = bundle.room.entryPoint(spawnId);
    if (!entry) {
        fail("spawn desconhecido na sala " + bundle.room.id + ": " + std::string(spawnId));
    }
    std::string error;
    SessionState progress;
    if (save && !progress.restore(save->session, error)) fail("save recusado: " + error);
    std::unique_ptr<Game> game;
    try {
        game = std::make_unique<Game>(std::move(bundle.room), std::move(progress));
    } catch (const std::exception& exception) {
        fail(std::string("sala recusada: ") + exception.what());
    }
    if (!game->setInteractions(std::move(bundle.interactions), error)
        || !game->setDialogues(std::move(bundle.dialogues), error)) {
        fail("regras ou dialogos recusados: " + error);
    }
    if (save) (void)game->placePlayerNear(save->feet, save->facing);
    else if (!spawnId.empty()) (void)game->placePlayerNear(entry->feet, entry->facing);
    game_ = std::move(game);
    processEvents(); // the RoomEntered of the first room: the shell's first autosave
}

void WalkthroughDriver::begin(std::string description) {
    trail_.push_back(std::to_string(trail_.size() + 1) + ") " + std::move(description));
}

void WalkthroughDriver::fail(const std::string& detail) {
    std::string message = "\nPercurso falhou no "
                          + (trail_.empty() ? std::string("inicio") : "passo " + trail_.back())
                          + "\n  " + detail;
    if (game_) message += "\n  estado: " + describeState();
    message += "\n  passos anteriores:";
    const std::size_t from = trail_.size() > 7 ? trail_.size() - 7 : 0;
    for (std::size_t i = from; i + 1 < trail_.size(); ++i) message += "\n    " + trail_[i];
    throw WalkthroughFailure(message);
}

template <typename T>
void WalkthroughDriver::expectEqual(const std::string_view what, const T& actual,
                                    const T& expected) {
    if (!(actual == expected)) {
        fail(std::string(what) + ": esperado " + text(expected) + ", obtido " + text(actual));
    }
    REQUIRE(actual == expected); // counted by doctest: a walkthrough is made of verifications
}

std::string WalkthroughDriver::describeState() const {
    const Scene& scene = game_->scene();
    std::string state = "sala=" + scene.room().id + " pes=(" + number(scene.player().feet().x)
                        + ", " + number(scene.player().feet().y)
                        + ") olhando=" + std::string(facingName(scene.player().facing()))
                        + " andando=" + (scene.player().moving() ? "sim" : "nao")
                        + " cutscene=" + (game_->inCutscene() ? "sim" : "nao")
                        + " menu=" + (game_->awaitingDialogueChoice() ? choiceIds() : "nao")
                        + " t=" + number(static_cast<float>(seconds())) + "s";
    const SessionSnapshot progress = game_->session().snapshot();
    state += " flags={";
    for (std::size_t i = 0; i < progress.flags.size(); ++i) {
        state += (i == 0 ? "" : ", ") + progress.flags[i].id + "="
                 + std::to_string(progress.flags[i].value);
    }
    state += "} itens=" + text(progress.items);
    if (!lines_.empty()) state += " ultima_fala=" + text(lines_.back());
    return state;
}

std::string WalkthroughDriver::choiceIds() const {
    std::vector<std::string> ids;
    for (const DialogueChoice* option : game_->dialogueOptions()) ids.push_back(option->id);
    return text(ids);
}

bool WalkthroughDriver::idle() const {
    return !game_->inCutscene() && !game_->scene().player().moving()
           && !game_->scene().pendingInteraction();
}

// Application::processGameEvents without the window: answers a room change by loading the
// room, runs the autosave policy and records what a test wants to assert on.
void WalkthroughDriver::processEvents() {
    game_->drainEvents(eventBuffer_);
    std::optional<RoomChangeRequested> nextRoom;
    bool enteredRoom = false;
    bool stablePoint = false;
    for (GameEvent& event : eventBuffer_) {
        if (const auto* line = std::get_if<LineSpoken>(&event)) {
            lines_.push_back(line->text);
        } else if (const auto* entered = std::get_if<RoomEntered>(&event)) {
            entered_.push_back(*entered);
            enteredRoom = true;
        } else if (auto* change = std::get_if<RoomChangeRequested>(&event)) {
            nextRoom = std::move(*change); // several in one drain: the last request wins
        } else if (std::holds_alternative<CutsceneEnded>(event)) {
            stablePoint = true;
        }
    }
    if (nextRoom) {
        if (!validIdentifier(nextRoom->roomId)
            || (!nextRoom->spawnId.empty() && !validIdentifier(nextRoom->spawnId))) {
            fail("troca de sala com destino invalido: " + nextRoom->roomId + " "
                 + nextRoom->spawnId);
        }
        RoomBundleResult loaded = library_.loadBundle(nextRoom->roomId + ".room");
        std::string error = loaded.errorFile + ": " + loaded.error;
        if (!loaded.bundle
            || !enterRoom(*game_, std::move(*loaded.bundle), nextRoom->spawnId, error)) {
            fail("a troca para " + nextRoom->roomId + " " + nextRoom->spawnId
                 + " falhou: " + error);
        }
        processEvents(); // the RoomEntered of the room just loaded
        return;
    }
    if (enteredRoom) {
        if (const SavedGame* save = autosave_.checkpoint(*game_)) {
            autosaves_.push_back(*save);
            autosave_.markWritten();
        }
    } else if (stablePoint) {
        autosave_.capture(*game_);
    }
}

void WalkthroughDriver::tick() {
    game_->tick(tickSeconds);
    ++ticks_;
    processEvents();
}

WalkthroughDriver& WalkthroughDriver::click(const float viewportX, const float viewportY) {
    begin("click viewport (" + number(viewportX) + ", " + number(viewportY) + ")");
    const SDL_FPoint world = game_->scene().camera().viewportToWorld({viewportX, viewportY}, 1.0F);
    game_->onClick(world);
    processEvents();
    return *this;
}

WalkthroughDriver& WalkthroughDriver::clickObject(const std::string_view objectId) {
    begin("click object " + std::string(objectId));
    const SceneObject* object = game_->scene().room().findObject(objectId);
    if (!object) fail("a sala nao tem o objeto " + std::string(objectId));
    const SDL_FPoint world{object->bounds.x + object->bounds.w * 0.5F,
                           object->bounds.y + object->bounds.h * 0.5F};
    const SDL_FPoint viewport = game_->scene().camera().worldToViewport(world, 1.0F);
    if (viewport.x < 0.0F || viewport.x >= 1280.0F || viewport.y < 0.0F || viewport.y >= 720.0F) {
        fail("o objeto " + std::string(objectId) + " esta fora da tela (viewport "
             + number(viewport.x) + ", " + number(viewport.y) + "): ande ate perto dele");
    }
    game_->onClick(world);
    processEvents();
    return *this;
}

WalkthroughDriver& WalkthroughDriver::skipLine() {
    begin("skip line (click)");
    game_->onClick(game_->scene().camera().viewportToWorld({640.0F, 360.0F}, 1.0F));
    processEvents();
    return *this;
}

WalkthroughDriver& WalkthroughDriver::choose(const std::string_view choiceId) {
    begin("choose " + std::string(choiceId));
    if (!game_->awaitingDialogueChoice()) fail("nao ha menu de escolhas na tela");
    const auto options = game_->dialogueOptions();
    const auto found = std::ranges::find_if(
            options, [&](const DialogueChoice* option) { return option->id == choiceId; });
    if (found == options.end())
        fail("a escolha " + std::string(choiceId) + " nao esta no menu " + choiceIds());
    if (!game_->chooseDialogueOption(static_cast<std::size_t>(found - options.begin()))) {
        fail("o jogo recusou a escolha " + std::string(choiceId));
    }
    processEvents();
    return *this;
}

WalkthroughDriver& WalkthroughDriver::use(const std::string_view itemId,
                                          const std::string_view objectId) {
    begin("use " + std::string(itemId) + " on " + std::string(objectId));
    if (!game_->useItemOnObject(itemId, objectId)) {
        fail("o jogo recusou o uso (item fora do inventario, objeto oculto, ou em cutscene)");
    }
    processEvents();
    return *this;
}

WalkthroughDriver& WalkthroughDriver::give(const std::string_view itemId) {
    begin("give " + std::string(itemId));
    game_->changeSession([&](SessionState& session) { session.addItem(itemId); });
    return *this;
}

WalkthroughDriver& WalkthroughDriver::setFlag(const std::string_view flagId, const int value) {
    begin("flag " + std::string(flagId) + " " + std::to_string(value));
    game_->changeSession([&](SessionState& session) {
        if (value == 0) session.clearFlag(flagId);
        else session.setFlag(flagId, value);
    });
    return *this;
}

WalkthroughDriver& WalkthroughDriver::goTo(const std::string_view roomId,
                                           const std::string_view spawnId) {
    begin("goto " + std::string(roomId) + (spawnId.empty() ? "" : " " + std::string(spawnId)));
    game_->requestRoomChange(std::string(roomId), std::string(spawnId));
    processEvents();
    return *this;
}

WalkthroughDriver& WalkthroughDriver::advanceTicks(const int ticks) {
    begin("advance " + std::to_string(ticks) + " ticks");
    for (int i = 0; i < ticks; ++i) tick();
    return *this;
}

WalkthroughDriver& WalkthroughDriver::advance(const float seconds) {
    begin("advance " + number(seconds) + " s");
    const int ticks = static_cast<int>(std::ceil(seconds * 60.0F - 0.001F));
    for (int i = 0; i < ticks; ++i) tick();
    return *this;
}

WalkthroughDriver& WalkthroughDriver::settle(const float limitSeconds) {
    begin("settle (until idle, limit " + number(limitSeconds) + " s)");
    const int limit = static_cast<int>(limitSeconds * 60.0F);
    for (int i = 0; i < limit; ++i) {
        if (idle()) return *this;
        if (game_->awaitingDialogueChoice()) {
            fail("a conversa espera uma escolha " + choiceIds() + ": use choose()");
        }
        tick();
    }
    if (idle()) return *this;
    fail("o jogo continuou ocupado depois de " + number(limitSeconds)
         + " s simulados (softlock?): cutscene=" + text(game_->inCutscene())
         + " andando=" + text(game_->scene().player().moving()));
}

WalkthroughDriver& WalkthroughDriver::waitForChoice(const float limitSeconds) {
    begin("wait for the dialogue menu (limit " + number(limitSeconds) + " s)");
    const int limit = static_cast<int>(limitSeconds * 60.0F);
    for (int i = 0; i < limit && !game_->awaitingDialogueChoice(); ++i) {
        if (idle()) fail("a conversa terminou sem mostrar o menu de escolhas");
        tick();
    }
    if (!game_->awaitingDialogueChoice()) fail("o menu de escolhas nao apareceu no tempo limite");
    return *this;
}

WalkthroughDriver& WalkthroughDriver::waitUntil(const std::string_view what,
                                                const std::function<bool()>& done,
                                                const float limitSeconds) {
    begin("wait until " + std::string(what));
    const int limit = static_cast<int>(limitSeconds * 60.0F);
    for (int i = 0; i < limit && !done(); ++i) tick();
    if (!done())
        fail("a condicao '" + std::string(what) + "' nao valeu em " + number(limitSeconds) + " s");
    return *this;
}

WalkthroughDriver& WalkthroughDriver::saveAndResume() {
    begin("save and resume save.slot");
    if (game_->inCutscene()) fail("so ha save num momento estavel (sem sequencia nem conversa)");
    if (!saveFolder_) saveFolder_ = std::make_unique<TempDirectory>("pointclick-walkthrough");
    const std::filesystem::path path = saveFolder_->path() / std::string(SaveFiles::slotName);
    const SavedGame saved = game_->savedGame();
    std::string error;
    if (!SaveFiles::save(path, saved, error)) fail("nao gravou o save: " + error);
    ResumeResult resumed = SaveFiles::resume(path, library_, RefusedSave::keep);
    if (!resumed.save || !resumed.bundle) fail("o save nao voltou: " + resumed.error);
    if (!(*resumed.save == saved)) fail("o save lido difere do gravado");
    startGame(std::move(*resumed.bundle), &*resumed.save, {});
    return *this;
}

WalkthroughDriver& WalkthroughDriver::expectRoom(const std::string_view roomId) {
    begin("expect room " + std::string(roomId));
    expectEqual("sala", game_->scene().room().id, std::string(roomId));
    return *this;
}

WalkthroughDriver& WalkthroughDriver::expectFeet(const float x, const float y,
                                                 const float tolerance) {
    begin("expect feet (" + number(x) + ", " + number(y) + ") +-" + number(tolerance));
    const SDL_FPoint feet = game_->scene().player().feet();
    if (std::abs(feet.x - x) > tolerance || std::abs(feet.y - y) > tolerance) {
        fail("pes: esperado (" + number(x) + ", " + number(y) + ") +-" + number(tolerance)
             + ", obtido (" + number(feet.x) + ", " + number(feet.y) + ")");
    }
    REQUIRE(true);
    return *this;
}

WalkthroughDriver& WalkthroughDriver::expectFacing(const Facing facing) {
    begin("expect facing " + std::string(facingName(facing)));
    expectEqual("direcao", game_->scene().player().facing(), facing);
    return *this;
}

WalkthroughDriver& WalkthroughDriver::expectFlag(const std::string_view flagId, const int value) {
    begin("expect flag " + std::string(flagId) + " = " + std::to_string(value));
    expectEqual("flag " + std::string(flagId), game_->session().flagValue(flagId), value);
    return *this;
}

WalkthroughDriver&
WalkthroughDriver::expectItems(const std::initializer_list<std::string_view> items) {
    begin("expect items " + text(toStrings(items)));
    expectEqual("inventario", game_->session().items(), toStrings(items));
    return *this;
}

WalkthroughDriver& WalkthroughDriver::expectObjectVisible(const std::string_view objectId,
                                                          const bool visible) {
    begin("expect object " + std::string(objectId) + (visible ? " visible" : " hidden"));
    expectEqual("objeto " + std::string(objectId) + " visivel",
                game_->scene().isObjectVisible(objectId), visible);
    return *this;
}

WalkthroughDriver& WalkthroughDriver::expectObjectSolid(const std::string_view objectId,
                                                        const bool solid) {
    begin("expect object " + std::string(objectId) + (solid ? " solid" : " not solid"));
    const SceneObject* object = game_->scene().room().findObject(objectId);
    if (!object) fail("a sala nao tem o objeto " + std::string(objectId));
    expectEqual("objeto " + std::string(objectId) + " solido",
                game_->scene().isObjectSolid(*object), solid);
    return *this;
}

WalkthroughDriver& WalkthroughDriver::expectObjectAnimation(const std::string_view objectId,
                                                            const std::string_view clip,
                                                            const bool finished) {
    begin("expect object " + std::string(objectId) + " clip " + std::string(clip)
          + (finished ? " finished" : " running"));
    const AnimationPlayback* playback = game_->scene().objectAnimation(objectId);
    if (!playback) fail("o objeto " + std::string(objectId) + " nao tem animacao");
    expectEqual("clip de " + std::string(objectId), std::string(playback->name()),
                std::string(clip));
    expectEqual("clip de " + std::string(objectId) + " terminou",
                game_->scene().animationFinished(objectId), finished);
    return *this;
}

WalkthroughDriver& WalkthroughDriver::expectCameraX(const float x, const float tolerance) {
    begin("expect camera x " + number(x) + " +-" + number(tolerance));
    const float actual = game_->scene().camera().position().x;
    if (std::abs(actual - x) > tolerance) {
        fail("camera x: esperado " + number(x) + " +-" + number(tolerance) + ", obtido "
             + number(actual));
    }
    REQUIRE(true);
    return *this;
}

WalkthroughDriver& WalkthroughDriver::expectIdle() {
    begin("expect idle");
    expectEqual("cena ociosa", idle(), true);
    return *this;
}

WalkthroughDriver&
WalkthroughDriver::expectLines(const std::initializer_list<std::string_view> lines) {
    begin("expect lines " + text(toStrings(lines)));
    expectEqual("falas", lines_, toStrings(lines));
    return *this;
}

WalkthroughDriver& WalkthroughDriver::expectLastLine(const std::string_view line) {
    begin("expect last line " + text(line));
    if (lines_.empty()) fail("nenhuma fala ate agora; esperado " + text(line));
    expectEqual("ultima fala", lines_.back(), std::string(line));
    return *this;
}

WalkthroughDriver&
WalkthroughDriver::expectChoices(const std::initializer_list<std::string_view> choiceIds) {
    begin("expect choices " + text(toStrings(choiceIds)));
    if (!game_->awaitingDialogueChoice()) fail("nao ha menu de escolhas na tela");
    std::vector<std::string> ids;
    for (const DialogueChoice* option : game_->dialogueOptions()) ids.push_back(option->id);
    expectEqual("opcoes do menu", ids, toStrings(choiceIds));
    return *this;
}

WalkthroughDriver& WalkthroughDriver::expectRoomsEntered(const std::size_t count) {
    begin("expect " + std::to_string(count) + " rooms entered");
    expectEqual("salas carregadas", static_cast<int>(entered_.size()), static_cast<int>(count));
    return *this;
}

WalkthroughDriver& WalkthroughDriver::expectLastAutosave(const std::string_view roomId,
                                                         const float x, const float y,
                                                         const Facing facing,
                                                         const float tolerance) {
    begin("expect last autosave in " + std::string(roomId));
    if (autosaves_.empty()) fail("o autosave nunca gravou");
    const SavedGame& save = autosaves_.back();
    expectEqual("sala do autosave", save.roomId, std::string(roomId));
    if (std::abs(save.feet.x - x) > tolerance || std::abs(save.feet.y - y) > tolerance) {
        fail("pes do autosave: esperado (" + number(x) + ", " + number(y) + "), obtido ("
             + number(save.feet.x) + ", " + number(save.feet.y) + ")");
    }
    REQUIRE(true);
    expectEqual("direcao do autosave", save.facing, facing);
    return *this;
}

WalkthroughDriver& WalkthroughDriver::clearLines() {
    begin("clear recorded lines");
    lines_.clear();
    return *this;
}
