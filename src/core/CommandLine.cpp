#include "core/CommandLine.hpp"

#include "game/Identifier.hpp"

#include <charconv>

CommandLineResult parseCommandLine(const std::span<const std::string_view> args) {
    CommandLineOptions options;
    bool roomsGiven = false;
    bool framesGiven = false;
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
        } else if (arg == "--screenshot") {
            if (!options.screenshot.empty()) return fail("Opcao repetida: --screenshot");
            if (i + 1 >= args.size() || !args[i + 1].ends_with(".png")) {
                return fail("--screenshot precisa de um arquivo .png");
            }
            const std::string_view utf8 = args[++i];
            options.screenshot = std::u8string(utf8.begin(), utf8.end());
        } else if (arg == "--frames") {
            if (!once(framesGiven)) return fail("Opcao repetida: --frames");
            constexpr int maximumFrames = 36000; // ten minutes at 60 Hz
            int frames = 0;
            const std::string_view text = i + 1 < args.size() ? args[i + 1] : std::string_view{};
            const auto [end, status] = std::from_chars(text.data(), text.data() + text.size(),
                                                       frames);
            if (text.empty() || status != std::errc{} || end != text.data() + text.size()
                || frames < 1 || frames > maximumFrames) {
                return fail("--frames precisa de um numero de 1 a 36000");
            }
            ++i;
            options.screenshotFrames = frames;
#if POINTCLICK_TOOLS
        } else if (arg == "--exec" || arg == "--exec-file") {
            if (i + 1 >= args.size() || args[i + 1].empty()) {
                return fail(
                        std::string(arg)
                        + (arg == "--exec" ? " precisa de um comando" : " precisa de um arquivo"));
            }
            const std::string_view value = args[++i];
            ExecStep step;
            if (arg == "--exec") step.command = value;
            else step.file = std::u8string(value.begin(), value.end());
            options.exec.push_back(std::move(step));
#endif
        } else {
            return fail("Opcao desconhecida: " + std::string(arg));
        }
    }
    if (options.check
        && (options.newGame || options.noSave || !options.startRoom.empty()
            || options.runsCommands())) {
        return fail("--check aceita apenas --rooms");
    }
    if (framesGiven && !options.takesScreenshot()) return fail("--frames exige --screenshot");
    if (options.takesScreenshot() && (options.check || options.runsCommands())) {
        return fail("--screenshot nao combina com --check nem --exec");
    }
    return {std::move(options), {}};
}

// --exec exists only in builds with the tools (POINTCLICK_TOOLS): a store build neither accepts
// nor advertises it.
#if POINTCLICK_TOOLS
#define POINTCLICK_EXEC_SYNOPSIS                                                                   \
    "     pointclick [--rooms <pasta>] [--room <sala> [spawn]] --exec <comando> | --exec-file "    \
    "<arquivo> ...\n"
#define POINTCLICK_EXEC_OPTIONS                                                                    \
    "  --exec <comando>      roda um comando do editor DEBUG sem janela (repetivel, em ordem);\n"  \
    "                        jogo novo que nunca le nem grava o save; sai com 1 se algum\n"        \
    "                        comando falhar\n"                                                     \
    "  --exec-file <arquivo> o mesmo, um comando por linha (linhas vazias e # sao ignoradas)\n"
#else
#define POINTCLICK_EXEC_SYNOPSIS ""
#define POINTCLICK_EXEC_OPTIONS ""
#endif

std::string_view commandLineUsage() {
    return "Uso: pointclick [--rooms <pasta>] [--new-game] [--no-save] [--room <sala> [spawn]]\n"
           "     pointclick [--rooms <pasta>] --check\n"
           "     pointclick [opcoes de jogo] --screenshot <arquivo.png> [--frames "
           "N]\n" POINTCLICK_EXEC_SYNOPSIS
           "  --rooms <pasta>       pasta das salas (padrao: assets/rooms junto do executavel)\n"
           "  --new-game            comeca do zero, sem retomar o save\n"
           "  --no-save             nao grava o autosave (o save existente fica intacto)\n"
           "  --room <sala> [spawn] comeca um jogo novo em <sala>.room, no [spawn] indicado\n"
           "  --check               valida salas, .rules, .dlg e change_room; sai sem abrir "
           "janela\n"
           "                        (arte ausente: PNG, corte fora da imagem ou fonte, sai como\n"
           "                        \"aviso:\" e nao muda o codigo de "
           "saida)\n"
           "  --screenshot <arq.png> roda N ticks de 60 Hz sem janela na tela, salva um frame em\n"
           "                        PNG e sai (nunca grava o autosave)\n"
           "  --frames N            ticks antes da captura (padrao 1, ate "
           "36000)\n" POINTCLICK_EXEC_OPTIONS;
}
