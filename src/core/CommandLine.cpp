#include "core/CommandLine.hpp"

#include "game/Identifier.hpp"

CommandLineResult parseCommandLine(const std::span<const std::string_view> args) {
    CommandLineOptions options;
    bool roomsGiven = false;
    const auto fail = [](std::string error) { return CommandLineResult{std::nullopt, std::move(error)}; };

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        const auto once = [&](bool& seen) {
            if (seen) return false;
            seen = true;
            return true;
        };
        if (arg == "--rooms") {
            if (roomsGiven) return fail("Opcao repetida: --rooms");
            if (i + 1 >= args.size() || args[i + 1].empty()) return fail("--rooms precisa de uma pasta");
            const std::string_view utf8 = args[++i];
            options.roomsDirectory = std::u8string(utf8.begin(), utf8.end());
            roomsGiven = true;
        } else if (arg == "--new-game") {
            if (!once(options.newGame)) return fail("Opcao repetida: --new-game");
        } else if (arg == "--no-save") {
            if (!once(options.noSave)) return fail("Opcao repetida: --no-save");
        } else if (arg == "--check") {
            if (!once(options.check)) return fail("Opcao repetida: --check");
        } else if (arg == "--room") {
            if (!options.startRoom.empty()) return fail("Opcao repetida: --room");
            if (i + 1 >= args.size() || args[i + 1].starts_with("--")) {
                return fail("--room precisa do id de uma sala");
            }
            const std::string_view room = args[++i];
            if (!validIdentifier(room)) return fail("Id de sala invalido: " + std::string(room));
            options.startRoom = room;
            // The spawn is optional: the next word, unless it is another option.
            if (i + 1 < args.size() && !args[i + 1].starts_with("--")) {
                const std::string_view spawn = args[++i];
                if (!validIdentifier(spawn)) return fail("Id de spawn invalido: " + std::string(spawn));
                options.startSpawn = spawn;
            }
        } else {
            return fail("Opcao desconhecida: " + std::string(arg));
        }
    }
    if (options.check && (options.newGame || options.noSave || !options.startRoom.empty())) {
        return fail("--check aceita apenas --rooms");
    }
    return {std::move(options), {}};
}

std::string_view commandLineUsage() {
    return "Uso: pointclick [--rooms <pasta>] [--new-game] [--no-save] [--room <sala> [spawn]]\n"
           "     pointclick [--rooms <pasta>] --check\n"
           "  --rooms <pasta>       pasta das salas (padrao: assets/rooms junto do executavel)\n"
           "  --new-game            comeca do zero, sem retomar o save\n"
           "  --no-save             nao grava o autosave (o save existente fica intacto)\n"
           "  --room <sala> [spawn] comeca um jogo novo em <sala>.room, no [spawn] indicado\n"
           "  --check               valida salas, .rules, .dlg e change_room; sai sem abrir janela\n";
}
