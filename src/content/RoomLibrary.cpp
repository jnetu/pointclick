#include "content/RoomLibrary.hpp"

#include "content/ActorFiles.hpp"
#include "content/DialogueFiles.hpp"
#include "content/InteractionFiles.hpp"

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

RoomLoadResult RoomLibrary::loadRoom(const std::string_view filename) const {
    return RoomFiles::load(roomsDirectory_ / std::filesystem::path(filename),
                           playerArt_.empty() ? nullptr : &playerArt_);
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

bool RoomLibrary::saveRoom(const std::string_view filename, const Room& room,
                           std::string& error) const {
    return RoomFiles::save(roomsDirectory_ / std::filesystem::path(filename), room, error,
                           playerArt_.empty() ? nullptr : &playerArt_);
}
