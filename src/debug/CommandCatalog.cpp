#include "debug/CommandCatalog.hpp"
#include "content/RoomLibrary.hpp"
#include "debug/CommandConsole.hpp"
#include "game/Game.hpp"
#include "game/Identifier.hpp"
#include "game/SessionState.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <optional>
#include <system_error>
#include <utility>

namespace {
bool finished(std::istringstream& input) {
    input >> std::ws;
    return input.eof();
}

// "<id>.room" with no folders; checked on the typed text, never through a native path string.
bool validFilename(const std::string_view name) {
    constexpr std::string_view extension = ".room";
    return name.size() > extension.size() && name.ends_with(extension)
           && validIdentifier(name.substr(0, name.size() - extension.size()));
}

// `show <name> <0|1>`: a new debug view is one line here plus its drawing in DebugOverlay.
struct ViewSwitch {
    std::string_view name;
    bool DebugViews::* enabled;
};
constexpr ViewSwitch kViewSwitches[]{
    {"route", &DebugViews::route},
    {"diagnostics", &DebugViews::diagnostics},
    {"hint", &DebugViews::hint},
};

// ---- Room editing (definition; never the progress) ----

bool setCommand(CommandContext& context) {
    std::string key;
    float value;
    if (!(context.args >> key >> value) || !finished(context.args)) {
        return context.fail("Use: set player.far_speed 140");
    }
    return context.console.setParameter(context.game, key, value, context.output);
}

bool teleportCommand(CommandContext& context) {
    SDL_FPoint point{};
    if (!(context.args >> point.x >> point.y) || !finished(context.args) || !std::isfinite(point.x)
        || !std::isfinite(point.y)) {
        return context.fail("Use: teleport 640 500");
    }
    if (!context.game.teleportPlayer(point, context.output)) return false;
    return context.done("Player reposicionado");
}

// move, size, base and solid: one field of one object of the room definition.
bool objectCommand(CommandContext& context) {
    const std::string_view verb = context.command.name;
    std::string id;
    context.args >> id;
    Room room = context.game.scene().room();
    const auto object = std::find_if(room.scenery.begin(), room.scenery.end(),
                                     [&](const auto& item) { return item.id == id; });
    if (object == room.scenery.end()) return context.fail("Objeto desconhecido: " + id);
    std::istringstream& input = context.args;
    bool parsed = false;
    if (verb == "move") parsed = static_cast<bool>(input >> object->bounds.x >> object->bounds.y);
    if (verb == "size") parsed = static_cast<bool>(input >> object->bounds.w >> object->bounds.h);
    if (verb == "base") {
        auto& base = object->collisionFootprint;
        parsed = static_cast<bool>(input >> base.x >> base.y >> base.w >> base.h);
    }
    if (verb == "solid") {
        int enabled = -1;
        parsed = static_cast<bool>(input >> enabled) && (enabled == 0 || enabled == 1);
        object->type = enabled == 1 ? SceneObjectType::solid : SceneObjectType::decoration;
    }
    if (!parsed || !finished(input)) {
        return context.fail("Use move/size ID X Y, base ID X Y W H, solid ID 0/1");
    }
    return context.console.commit(context.game, std::move(room), context.output);
}

bool showCommand(CommandContext& context) {
    std::string view;
    int enabled = -1;
    bool* target = nullptr;
    if (context.args >> view >> enabled && finished(context.args)
        && (enabled == 0 || enabled == 1)) {
        for (const ViewSwitch& entry : kViewSwitches) {
            if (entry.name == view) target = &(context.console.views().*entry.enabled);
        }
    }
    if (!target) {
        std::string names;
        for (const ViewSwitch& entry : kViewSwitches) {
            names += (names.empty() ? "" : "|") + std::string(entry.name);
        }
        return context.fail("Use: show " + names + " 0/1");
    }
    *target = enabled == 1;
    return context.done(view + (enabled == 1 ? ": visivel" : ": oculto"));
}

// save and load: <id>.room in the room library's folder.
bool fileCommand(CommandContext& context) {
    const bool loading = context.command.name == "load";
    std::istringstream& input = context.args;
    std::string name;
    std::string spawn; // load only: enter through a [spawn] of the room
    const bool named = static_cast<bool>(input >> name);
    if (loading && named && !finished(input)) input >> spawn;
    if (!named || !finished(input) || !validFilename(name)
        || (!spawn.empty() && !validIdentifier(spawn))) {
        return context.fail(loading ? "Use: load nome.room [spawn] (sem pastas)"
                                    : "Use: save nome.room (sem pastas)");
    }
    const RoomLibrary* library = context.console.library();
    if (!library) return context.fail("Pasta de salas nao configurada");
    if (!loading) {
        if (!library->saveRoom(name, context.game.scene().room(), context.output)) return false;
        return context.done("Salvo: " + name);
    }
    RoomBundleResult loaded = library->loadBundle(name);
    if (!loaded.bundle) return context.fail(std::move(loaded.error));
    if (!context.console.commit(context.game, std::move(loaded.bundle->room), context.output, true,
                                std::move(loaded.bundle->interactions),
                                std::move(loaded.bundle->dialogues), spawn)) {
        return false;
    }
    (void)context.console.reloadArt(); // the room's PNGs may have changed on disk too
    return context.done("Sala carregada: " + name + (spawn.empty() ? "" : " em " + spawn));
}

bool reloadCommand(CommandContext& context) {
    // Art only: the room, the feet and the progress stay as they are.
    if (!finished(context.args)) return context.fail("Comando desconhecido. Digite help.");
    if (!context.console.reloadArt()) return context.fail("Recarga de sprites indisponivel");
    return context.done("Sprites recarregados do disco");
}

bool undoCommand(CommandContext& context) {
    if (!finished(context.args)) return context.fail("Comando desconhecido. Digite help.");
    return context.console.undo(context.game, context.output);
}

bool resetCommand(CommandContext& context) {
    if (!finished(context.args)) return context.fail("Comando desconhecido. Digite help.");
    return context.console.reset(context.game, context.output);
}

// ---- Session (progress), always through Game::changeSession ----

// The room in play, or another one read through the library (a copy: tools only). Empty with
// `error` set when it does not load.
std::optional<Room> targetRoom(CommandContext& context, const std::string& roomId,
                               std::string& error) {
    if (roomId == context.game.scene().room().id) return context.game.scene().room();
    const RoomLibrary* library = context.console.library();
    if (!library) {
        error = "Pasta de salas nao configurada";
        return std::nullopt;
    }
    RoomLoadResult loaded = library->loadRoom(roomId + ".room");
    if (!loaded.room) error = "Sala desconhecida: " + roomId + " (" + loaded.error + ")";
    return std::move(loaded.room);
}

// goto and newgame: the room loads and has the spawn (empty = playerStart). Empty = valid.
std::string entryError(CommandContext& context, const std::string& roomId,
                       const std::string& spawnId) {
    std::string error;
    const std::optional<Room> room = targetRoom(context, roomId, error);
    if (!room) return error;
    if (!room->entryPoint(spawnId)) return "Spawn desconhecido na sala " + roomId + ": " + spawnId;
    return {};
}

bool flagCommand(CommandContext& context) {
    std::string id;
    std::string text;
    if (!(context.args >> id >> text) || !finished(context.args)) return context.usage();
    if (!validIdentifier(id)) return context.fail("Flag invalida: " + id);
    long long value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error == std::errc::invalid_argument || end != text.data() + text.size()) {
        return context.usage();
    }
    if (error == std::errc::result_out_of_range || value < -kMaxFlagValue
        || value > kMaxFlagValue) {
        return context.fail("Valor fora do limite de +-" + std::to_string(kMaxFlagValue) + ": "
                            + text);
    }
    const int stored = static_cast<int>(value);
    context.game.changeSession([&](SessionState& session) { session.setFlag(id, stored); });
    if (stored == 0) return context.done("Flag apagada: " + id);
    return context.done("Flag " + id + " = " + std::to_string(stored));
}

// give and take: one item of the inventory.
bool itemCommand(CommandContext& context) {
    const bool giving = context.command.name == "give";
    std::string item;
    if (!(context.args >> item) || !finished(context.args)) return context.usage();
    if (!validIdentifier(item)) return context.fail("Item invalido: " + item);
    if (giving == context.game.session().hasItem(item)) {
        return context.fail((giving ? "Item ja esta no inventario: " : "Item fora do inventario: ")
                            + item);
    }
    context.game.changeSession([&](SessionState& session) {
        if (giving) (void)session.addItem(item);
        else (void)session.removeItem(item);
    });
    return context.done((giving ? "Item recebido: " : "Item removido: ") + item);
}

// showobj and hideobj: the session's visibility override of <sala.objeto>, or of an object of
// the room in play. The names mirror the show_object/hide_object effects; `show` is taken by
// the debug views.
bool visibilityCommand(CommandContext& context) {
    const bool visible = context.command.name == "showobj";
    std::string reference;
    if (!(context.args >> reference) || !finished(context.args)) return context.usage();
    const std::size_t dot = reference.find('.');
    std::string roomId = context.game.scene().room().id;
    std::string objectId = reference;
    if (dot != std::string::npos) {
        roomId = reference.substr(0, dot);
        objectId = reference.substr(dot + 1);
    }
    if (!validIdentifier(roomId) || !validIdentifier(objectId)) {
        return context.fail("Objeto invalido: " + reference);
    }
    std::string error;
    const std::optional<Room> room = targetRoom(context, roomId, error);
    if (!room) return context.fail(std::move(error));
    if (!room->findObject(objectId)) {
        return context.fail("Objeto desconhecido: " + roomId + "." + objectId);
    }
    context.game.changeSession(
            [&](SessionState& session) { session.setObjectVisible(roomId, objectId, visible); });
    return context.done(roomId + "." + objectId + (visible ? ": visivel" : ": oculto"));
}

bool useCommand(CommandContext& context) {
    std::string item;
    std::string objectId;
    if (!(context.args >> item >> objectId) || !finished(context.args)) return context.usage();
    if (!validIdentifier(item)) return context.fail("Item invalido: " + item);
    Game& game = context.game;
    if (!game.scene().room().findObject(objectId)) {
        return context.fail("Objeto desconhecido: " + objectId);
    }
    if (!game.session().hasItem(item)) {
        return context.fail("Item fora do inventario: " + item + " (give " + item + ")");
    }
    if (!game.scene().isObjectVisible(objectId)) return context.fail("Objeto oculto: " + objectId);
    if (game.inCutscene()) return context.fail("Sequencia em andamento; tente de novo depois");
    if (!game.useItemOnObject(item, objectId)) {
        return context.fail("Nada aconteceu: nenhuma regra [use_item " + item + " " + objectId
                            + "] se aplica, ou o objeto esta fora de alcance");
    }
    // The player may still be walking there: the rule runs on arrival, in the game loop (or
    // when the headless runner settles the game).
    return context.done("Usando " + item + " em " + objectId);
}

bool gotoCommand(CommandContext& context) {
    std::string roomId;
    std::string spawnId;
    if (!(context.args >> roomId)) return context.usage();
    if (!finished(context.args)) context.args >> spawnId;
    if (!finished(context.args) || !validIdentifier(roomId)
        || (!spawnId.empty() && !validIdentifier(spawnId))) {
        return context.usage();
    }
    if (std::string error = entryError(context, roomId, spawnId); !error.empty()) {
        return context.fail(std::move(error));
    }
    context.game.requestRoomChange(roomId, spawnId);
    return context.done("Indo para " + roomId + (spawnId.empty() ? "" : " em " + spawnId));
}

bool newGameCommand(CommandContext& context) {
    if (!finished(context.args)) return context.usage();
    const std::string& roomId = context.console.startRoom();
    const std::string& spawnId = context.console.startSpawn();
    // Checked first: a start room that does not load must not cost the progress.
    if (std::string error = entryError(context, roomId, spawnId); !error.empty()) {
        return context.fail(std::move(error));
    }
    context.game.startNewGame(roomId, spawnId);
    return context.done("Novo jogo: progresso zerado, voltando a " + roomId);
}

std::string onOff(const std::optional<bool> value) {
    return *value ? "1" : "0";
}

bool stateCommand(CommandContext& context) {
    if (!finished(context.args)) return context.usage();
    // snapshot() orders every list (items keep the inventory order), so the same progress
    // always prints the same text.
    const SessionSnapshot snapshot = context.game.session().snapshot();
    std::string text = "sala: " + context.game.scene().room().id;
    const auto list = [&text](const std::string_view title,
                              const std::vector<std::string>& entries) {
        text += "\n" + std::string(title) + ":";
        if (entries.empty()) text += " -";
        for (std::size_t i = 0; i < entries.size(); ++i) text += (i ? ", " : " ") + entries[i];
    };
    std::vector<std::string> entries;
    for (const SessionSnapshot::Flag& flag : snapshot.flags) {
        entries.push_back(flag.id + "=" + std::to_string(flag.value));
    }
    list("flags", entries);
    list("itens", snapshot.items);
    entries.clear();
    for (const SessionSnapshot::ObjectOverride& object : snapshot.objects) {
        std::string entry = object.roomId + "." + object.objectId;
        if (object.visible) entry += " visible=" + onOff(object.visible);
        if (object.solidOverride) entry += " solid=" + onOff(object.solidOverride);
        if (object.animation) entry += " animation=" + *object.animation;
        entries.push_back(std::move(entry));
    }
    list("objetos", entries);
    entries.clear();
    for (const SessionSnapshot::DialogueChoice& choice : snapshot.dialogueChoices) {
        entries.push_back(choice.roomId + "/" + choice.nodeId + "/" + choice.choiceId);
    }
    list("escolhas", entries);
    return context.done(std::move(text));
}

std::string commandLine(const CommandDescriptor& command) {
    return std::string(command.usage) + " -- " + std::string(command.description);
}

bool helpCommand(CommandContext& context) {
    std::string name;
    context.args >> name;
    if (!finished(context.args)) return context.usage();
    if (!name.empty()) {
        const CommandDescriptor* command = findCommand(name);
        if (!command) return context.fail("Comando desconhecido: " + name + ". Digite help.");
        return context.done(commandLine(*command));
    }
    std::string text = "Comandos: " + commandNames();
    for (const CommandDescriptor& command : commandCatalog()) text += "\n" + commandLine(command);
    return context.done(std::move(text));
}

// Order = the order of `help` and of the panel's list: room editing, then the session.
constexpr CommandDescriptor kCommands[]{
    {"set", "set <parametro> <valor>", "Muda um parametro da sala (a lista do painel)", setCommand},
    {"teleport", "teleport <x> <y>", "Poe os pes do player num ponto caminhavel", teleportCommand},
    {"move", "move <objeto> <x> <y>", "Move o objeto na definicao da sala", objectCommand},
    {"size", "size <objeto> <w> <h>", "Muda largura e altura do objeto", objectCommand},
    {"base", "base <objeto> <x> <y> <w> <h>", "Base de colisao do objeto, relativa a ele",
     objectCommand},
    {"solid", "solid <objeto> <0|1>", "Objeto solido (1) ou decoracao (0) na definicao",
     objectCommand},
    {"show", "show <visao> <0|1>", "Liga/desliga uma visao de debug: route, diagnostics, hint",
     showCommand},
    {"save", "save <nome>.room", "Grava a sala editada na pasta das salas", fileCommand},
    {"load", "load <nome>.room [spawn]", "Carrega sala, .rules e .dlg do disco, no spawn",
     fileCommand},
    {"reload", "reload", "Rele os PNGs do disco sem mexer na sala", reloadCommand},
    {"undo", "undo", "Desfaz a ultima edicao aplicada", undoCommand},
    {"reset", "reset", "Volta a sala como o jogo a carregou (ou o editor a encontrou)",
     resetCommand},
    {"flag", "flag <id> <n>", "Define a flag/contador (0 apaga; limite +-1000000000)", flagCommand},
    {"give", "give <item>", "Poe o item no inventario", itemCommand},
    {"take", "take <item>", "Tira o item do inventario", itemCommand},
    {"showobj", "showobj <sala.objeto|objeto>", "Mostra o objeto (visibilidade da sessao)",
     visibilityCommand},
    {"hideobj", "hideobj <sala.objeto|objeto>", "Esconde o objeto (visibilidade da sessao)",
     visibilityCommand},
    {"use", "use <item> <objeto>", "Usa um item do inventario no objeto ([use_item])", useCommand},
    {"goto", "goto <sala> [spawn]", "Troca de sala como change_room, mantendo o progresso",
     gotoCommand},
    {"newgame", "newgame", "Recomeca do zero na sala inicial, como --new-game", newGameCommand},
    {"state", "state", "Lista flags, itens, objetos alterados e escolhas de dialogo", stateCommand},
    {"help", "help [comando]", "Lista os comandos, ou o uso de um", helpCommand},
};
} // namespace

bool CommandContext::usage() const {
    return fail("Use: " + std::string(command.usage));
}

bool CommandContext::fail(std::string text) const {
    output = std::move(text);
    return false;
}

bool CommandContext::done(std::string text) const {
    output = std::move(text);
    return true;
}

std::span<const CommandDescriptor> commandCatalog() {
    return kCommands;
}

const CommandDescriptor* findCommand(const std::string_view name) {
    for (const CommandDescriptor& command : kCommands) {
        if (command.name == name) return &command;
    }
    return nullptr;
}

std::string commandNames() {
    std::string names;
    for (const CommandDescriptor& command : kCommands) {
        names += (names.empty() ? "" : " ") + std::string(command.name);
    }
    return names;
}
