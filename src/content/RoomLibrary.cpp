#include "content/RoomLibrary.hpp"

#include "content/ActorFiles.hpp"
#include "content/DialogueFiles.hpp"
#include "content/InteractionFiles.hpp"
#include "content/TextFormat.hpp"

#include <system_error>
#include <utility>

bool RoomLibrary::open(std::filesystem::path roomsDirectory, std::string& error) {
    roomsDirectory_ = std::move(roomsDirectory);
    playerArt_.clear();
    const auto actorPath = playerActorPath();
    std::error_code ec;
    if (!std::filesystem::exists(actorPath, ec)) {
        error.clear();
        return true;
    }
    ActorLoadResult actor = ActorFiles::load(actorPath);
    if (!actor.animations) {
        error = std::move(actor.error);
        return false;
    }
    playerArt_ = std::move(*actor.animations);
    error.clear();
    return true;
}

std::filesystem::path RoomLibrary::spritesDirectory() const {
    return roomsDirectory_.parent_path() / "sprites";
}

std::filesystem::path RoomLibrary::playerActorPath() const {
    return roomsDirectory_.parent_path() / "actors" / "player.actor";
}

std::string RoomLibrary::roomIdFor(const std::string_view filename) {
    return TextFormat::pathToUtf8(std::filesystem::path(filename).stem());
}

RoomLoadResult RoomLibrary::loadRoom(const std::string_view filename) const {
    RoomLoadResult loaded = RoomFiles::load(roomsDirectory_ / std::filesystem::path(filename),
                                            playerArt_.empty() ? nullptr : &playerArt_);
    // change_room finds rooms by file name and SessionState keys progress by Room::id, so the
    // two must agree; otherwise progress would be stored under another room's id.
    if (const std::string expected = roomIdFor(filename); loaded.room && loaded.room->id != expected) {
        return {std::nullopt, "id = " + loaded.room->id + " nao corresponde ao arquivo "
                                      + TextFormat::pathToUtf8(std::filesystem::path(filename).filename())
                                      + " (use id = " + expected + ")"};
    }
    return loaded;
}

RoomBundleResult RoomLibrary::loadBundle(const std::string_view filename) const {
    const auto roomPath = roomsDirectory_ / std::filesystem::path(filename);
    RoomLoadResult room = loadRoom(filename);
    if (!room.room) return {std::nullopt, std::move(room.error)};
    InteractionLoadResult rules = InteractionFiles::loadOptionalForRoom(roomPath, &*room.room);
    if (!rules.interactions) return {std::nullopt, std::move(rules.error)};
    DialogueLoadResult dialogues = DialogueFiles::loadOptionalForRoom(roomPath, &*room.room);
    if (!dialogues.dialogues) return {std::nullopt, std::move(dialogues.error)};
    if (std::string missing = dialogueReferencesError(*rules.interactions, *dialogues.dialogues);
        !missing.empty()) {
        return {std::nullopt, std::move(missing)};
    }
    return {RoomBundle{std::move(*room.room), std::move(*rules.interactions),
                       std::move(*dialogues.dialogues)},
            {}};
}

std::string RoomLibrary::transitionsError(const RoomBundle& bundle) const {
    const auto check = [&](const std::vector<InteractionEffect>& effects) -> std::string {
        for (const InteractionEffect& effect : effects) {
            if (!effect.effect || effect.effect->keyword != "change_room" || effect.args.empty()) {
                continue;
            }
            const std::string& roomId = effect.args[0];
            const RoomLoadResult target = loadRoom(roomId + ".room");
            if (!target.room) return "change_room " + roomId + ": " + target.error;
            if (effect.args.size() > 1 && !target.room->findSpawn(effect.args[1])) {
                return "change_room " + roomId + ": spawn desconhecido " + effect.args[1];
            }
        }
        return {};
    };
    for (const InteractionRule& rule : bundle.interactions.rules) {
        if (std::string error = check(rule.effects); !error.empty()) return error;
    }
    for (const DialogueNode& node : bundle.dialogues.nodes) {
        if (std::string error = check(node.lines); !error.empty()) return error;
        for (const DialogueChoice& choice : node.choices) {
            if (std::string error = check(choice.effects); !error.empty()) return error;
        }
    }
    return {};
}

bool RoomLibrary::saveRoom(const std::string_view filename, const Room& room,
                           std::string& error) const {
    // Saving under another name makes a copy that is that room: its id follows the file.
    Room named = room;
    named.id = roomIdFor(filename);
    return RoomFiles::save(roomsDirectory_ / std::filesystem::path(filename), named, error,
                           playerArt_.empty() ? nullptr : &playerArt_);
}
