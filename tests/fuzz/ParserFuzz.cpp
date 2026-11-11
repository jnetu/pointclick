// Deterministic mutation fuzz of the content parsers (.room, .rules, .dlg, .actor, save.slot).
// Not a doctest and not in CTest: build it with -DPOINTCLICK_ENABLE_FUZZ=ON under ASan + UBSan and
// run it by hand. A fixed seed mutates the shipped assets plus a few snippets with the newer
// vocabulary; every parser must answer "accepted" or "refused" without crashing. What an input
// means is not checked here (the test suite does that), only that no input is undefined behaviour.
//
//   pointclick_parser_fuzz [iterations per seed file = 2000] [seed = 20261111]
#include "content/ActorFiles.hpp"
#include "content/DialogueFiles.hpp"
#include "content/InteractionFiles.hpp"
#include "content/RoomFiles.hpp"
#include "content/SaveFiles.hpp"
#include "content/TextFormat.hpp"
#include "support/TempDirectory.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {
enum class Format : std::uint8_t { room, rules, dialogue, actor, save };

struct Seed {
    Format format;
    std::string name;
    std::string text;
    std::size_t room = 0; // validates the rules/dialogues: 0 first, 1 gallery, 2 first + extras
};

// Vocabulary added after the last manual fuzz: signed integers, facing, wildcards, flip_x,
// set_animation and the version 3 save keys. The assets do not use all of it yet.
constexpr std::string_view kRulesExtras
        = "[interact *]\nflag_at_least knocks -3\nadd_flag knocks -1000000\nface left\n"
          "say \"Nada \\\"aqui\\\".\"\n\n[use_item * red_box]\nflag_equals knocks 999999\n"
          "set_animation porta_obj open\n\n[use_item brass_key *]\nadd_flag knocks 1\nface up\n\n"
          "[use_item * *]\nwait 0.5\nchange_room gallery porta\n";
constexpr std::string_view kRoomExtras
        = "\n[spawn porta]\nfeet = 200, 500\nfacing = right\n\n[object porta_obj]\ntype = "
          "decoration\n"
          "bounds = 100, 300, 64, 128\nanimation.initial = idle\nanimation.idle.file = p.png\n"
          "animation.open.file = p.png\nanimation.open.frame_width = 64\n"
          "animation.open.frame_height = 128\nanimation.open.frames = 6\nanimation.open.fps = 8\n"
          "animation.open.loop = 0\nanimation.open.flip_x = 1\n";
// Belongs to [room], so it goes in front of the first player key of the shipped room.
constexpr std::string_view kRoomPlayerExtra = "player.left.flip_x = 1\n";
constexpr std::string_view kSaveSeed
        = "version = 3\nroom = first\nfeet = 220.5, 460.25\nfacing = left\nflag.knocks = "
          "-1000000000\n"
          "flag.found_brass_key = 1\nitem = brass_key\nobject.first.red_box.visible = 0\n"
          "object.first.red_box.solid = 0\nobject.first.red_box.animation = open\n"
          "dialogue = gallery/mesa/gavetas\n";

// Tokens spliced into the text: boundaries of every numeric field and pieces of the syntax.
constexpr std::array<std::string_view, 28> kDictionary{"2147483647",
                                                       "-2147483648",
                                                       "2147483648",
                                                       "-1",
                                                       "0",
                                                       "1e39",
                                                       "-1e39",
                                                       "nan",
                                                       "inf",
                                                       "0.0000001",
                                                       "99999999999999999999",
                                                       "+7",
                                                       "*",
                                                       "[",
                                                       "]",
                                                       "\"",
                                                       "\\",
                                                       "=",
                                                       ",",
                                                       "#",
                                                       "\n",
                                                       "\r\n",
                                                       "\xEF\xBB\xBF",
                                                       "left",
                                                       "flip_x = 1",
                                                       "[use_item * *]",
                                                       "version = 4",
                                                       "\t"};

using Random = std::mt19937; // same sequence on every platform for a given seed

std::size_t pick(Random& random, const std::size_t count) {
    return count == 0 ? 0 : static_cast<std::size_t>(random() % count);
}

std::vector<std::string> lines(const std::string& text) {
    std::vector<std::string> result(1);
    for (const char ch : text) {
        if (ch == '\n') result.emplace_back();
        else result.back().push_back(ch);
    }
    return result;
}

std::string join(const std::vector<std::string>& all) {
    std::string result;
    for (std::size_t i = 0; i < all.size(); ++i) {
        if (i > 0) result.push_back('\n');
        result += all[i];
    }
    return result;
}

void mutateOnce(Random& random, std::string& text, const std::string& other) {
    const std::size_t at = pick(random, text.size() + 1);
    switch (random() % 9) {
    case 0: // flip a bit
        if (!text.empty()) {
            char& ch = text[pick(random, text.size())];
            ch = static_cast<char>(static_cast<unsigned char>(ch) ^ (1U << (random() % 8)));
        }
        break;
    case 1: // any byte, NUL and high bytes included
        text.insert(at, 1, static_cast<char>(random() % 256));
        break;
    case 2: // drop a short run
        if (!text.empty()) text.erase(pick(random, text.size()), 1 + random() % 8);
        break;
    case 3: // dictionary token
        text.insert(at, kDictionary[pick(random, kDictionary.size())]);
        break;
    case 4: // truncate
        text.resize(at);
        break;
    case 5: { // replace the digits under the cursor with a boundary value
        std::size_t begin = text.find_first_of("0123456789", at);
        if (begin == std::string::npos) break;
        const std::size_t end = std::min(text.find_first_not_of("0123456789.", begin), text.size());
        text.replace(begin, end - begin, kDictionary[pick(random, 11)]);
        break;
    }
    default: { // line edits: duplicate, drop, swap, or bring a line from another seed file
        std::vector<std::string> all = lines(text);
        const std::size_t a = pick(random, all.size());
        const std::size_t b = pick(random, all.size());
        const unsigned edit = random() % 4;
        if (edit == 0) all.insert(all.begin() + static_cast<std::ptrdiff_t>(b), all[a]);
        else if (edit == 1 && all.size() > 1)
            all.erase(all.begin() + static_cast<std::ptrdiff_t>(a));
        else if (edit == 2) std::swap(all[a], all[b]);
        else {
            const std::vector<std::string> donor = lines(other);
            all.insert(all.begin() + static_cast<std::ptrdiff_t>(a),
                       donor[pick(random, donor.size())]);
        }
        text = join(all);
        break;
    }
    }
}

struct Counters {
    std::size_t accepted = 0;
    std::size_t refused = 0;
};

// Runs the parser of the format; the writer runs too when the input is accepted.
void parseOne(const Format format, const std::string& text, const std::filesystem::path& scratch,
              const Room& room, const AnimationSet& playerArt, Counters& counters,
              std::string* refusal = nullptr) {
    bool accepted = false;
    std::string error;
    std::string reason;
    switch (format) {
    case Format::room: { // RoomFiles reads from a path only
        const auto file = scratch / "fuzz.room";
        { std::ofstream(file, std::ios::binary | std::ios::trunc) << text; }
        const RoomLoadResult result = RoomFiles::load(file, &playerArt);
        accepted = result.room.has_value();
        reason = result.error;
        if (accepted) (void)RoomFiles::save(scratch / "out.room", *result.room, error, &playerArt);
        break;
    }
    case Format::rules: {
        (void)InteractionFiles::parse(text); // without cross references
        const InteractionLoadResult result = InteractionFiles::parse(text, &room);
        accepted = result.interactions.has_value();
        reason = result.error;
        if (accepted) {
            (void)InteractionFiles::save(scratch / "out.rules", *result.interactions, error, &room);
        }
        break;
    }
    case Format::dialogue: {
        (void)DialogueFiles::parse(text);
        const DialogueLoadResult result = DialogueFiles::parse(text, &room);
        accepted = result.dialogues.has_value();
        reason = result.error;
        if (accepted)
            (void)DialogueFiles::save(scratch / "out.dlg", *result.dialogues, error, &room);
        break;
    }
    case Format::actor: {
        const ActorLoadResult result = ActorFiles::parse(text);
        accepted = result.animations.has_value();
        reason = result.error;
        break;
    }
    case Format::save: {
        const SaveLoadResult result = SaveFiles::parse(text);
        accepted = result.save.has_value();
        reason = result.error;
        if (accepted) (void)SaveFiles::parse(SaveFiles::serialize(*result.save));
        break;
    }
    }
    ++(accepted ? counters.accepted : counters.refused);
    if (refusal) *refusal = accepted ? std::string() : reason;
}

std::string readAsset(const std::filesystem::path& file) {
    const TextFormat::TextLoad loaded = TextFormat::readFile(file);
    if (!loaded.text) {
        std::fprintf(stderr, "fuzz: %s\n", loaded.error.c_str());
        std::exit(2);
    }
    return *loaded.text;
}
} // namespace

int main(const int argc, char** argv) {
    const unsigned long iterations = argc > 1 ? std::strtoul(argv[1], nullptr, 10) : 2000UL;
    const unsigned long seedValue = argc > 2 ? std::strtoul(argv[2], nullptr, 10) : 20261111UL;
    const std::filesystem::path assets = std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets";

    std::vector<Seed> seeds;
    for (const std::string_view name : {"first.room", "gallery.room"}) {
        seeds.push_back({Format::room, std::string(name), readAsset(assets / "rooms" / name)});
    }
    std::string extendedRoom = seeds[0].text + std::string(kRoomExtras);
    if (const std::size_t at = extendedRoom.find("player."); at != std::string::npos) {
        extendedRoom.insert(at, kRoomPlayerExtra);
    }
    seeds.push_back({Format::room, "first.room+extras", extendedRoom});
    seeds.push_back({Format::rules, "first.rules", readAsset(assets / "rooms/first.rules")});
    seeds.push_back({Format::rules, "gallery.rules", readAsset(assets / "rooms/gallery.rules"), 1});
    seeds.push_back({Format::rules, "extras.rules", std::string(kRulesExtras), 2});
    seeds.push_back({Format::dialogue, "gallery.dlg", readAsset(assets / "rooms/gallery.dlg"), 1});
    seeds.push_back({Format::actor, "player.actor", readAsset(assets / "actors/player.actor")});
    seeds.push_back({Format::save, "save.slot", std::string(kSaveSeed)});

    // The real rooms and player art: mutated rules and dialogues are validated against them.
    const AnimationSet playerArt = ActorFiles::parse(readAsset(assets / "actors/player.actor"))
                                           .animations.value_or(AnimationSet{});
    const TempDirectory scratch("pointclick-fuzz");
    { std::ofstream(scratch.path() / "extended.room", std::ios::binary) << extendedRoom; }
    const std::array<RoomLoadResult, 3> rooms{
        RoomFiles::load(assets / "rooms/first.room", &playerArt),
        RoomFiles::load(assets / "rooms/gallery.room", &playerArt),
        RoomFiles::load(scratch.path() / "extended.room", &playerArt)};
    for (const RoomLoadResult& loaded : rooms) {
        if (!loaded.room) {
            std::fprintf(stderr, "fuzz: sala de referencia recusada: %s\n", loaded.error.c_str());
            return 2;
        }
    }

    Random random(static_cast<Random::result_type>(seedValue));
    Counters total;
    for (std::size_t index = 0; index < seeds.size(); ++index) {
        const Seed& seed = seeds[index];
        const Room& room = *rooms[seed.room].room;
        Counters counters;
        std::string refusal;
        parseOne(seed.format, seed.text, scratch.path(), room, playerArt, counters, &refusal);
        // A seed the parser refuses only exercises the error path: say why, so it gets fixed.
        const std::size_t seedAccepted = counters.accepted;
        if (seedAccepted == 0)
            std::printf("%s: seed refused: %s\n", seed.name.c_str(), refusal.c_str());
        for (unsigned long i = 0; i < iterations; ++i) {
            std::string text = seed.text;
            const Seed& other = seeds[pick(random, seeds.size())];
            for (unsigned edits = 1 + random() % 4; edits > 0; --edits) {
                mutateOnce(random, text, other.text);
            }
            parseOne(seed.format, text, scratch.path(), room, playerArt, counters);
        }
        std::printf("%-18s seed %s, mutants: %zu accepted, %zu refused\n", seed.name.c_str(),
                    seedAccepted == 1 ? "accepted" : "refused", counters.accepted - seedAccepted,
                    counters.refused - (1 - seedAccepted));
        total.accepted += counters.accepted;
        total.refused += counters.refused;
    }
    std::printf("fuzz: %zu inputs (%zu accepted, %zu refused), seed %lu, no crash\n",
                total.accepted + total.refused, total.accepted, total.refused, seedValue);
    return 0;
}
