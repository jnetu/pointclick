#include "support/WalkthroughDriver.hpp"

#include "content/SaveFiles.hpp"
#include "game/SessionState.hpp"

#include <doctest/doctest.h>

#include <cmath>

// F3: one case per playground room, played from the shipped files (assets/rooms) with the
// headless WalkthroughDriver: clicks, simulated time, then the state the room must reach.
// To add a walkthrough: WalkthroughDriver play("<room>"), the steps (click, clickObject, use,
// choose, advance, settle...), then the expectations. A failure names the step and shows what
// was obtained against what was expected.
//
// A scenario that needs pixels (flip_x) renders the driver's game in SpriteTests.cpp.

// first <-> gallery: change_room with [spawn], the arrival point and facing, and the autosave
// that every room entered writes.
TEST_CASE("testWalkthroughFirstAndGallery") {
    WalkthroughDriver play("first");
    play.expectRoom("first").expectRoomsEntered(1);
    play.expectLastAutosave("first", 640, 360, Facing::down); // the start room's own entry

    play.clickObject("door_gallery").settle();
    play.expectLines({"Vamos ver a galeria."}).expectRoom("gallery");
    play.expectFeet(200, 300).expectFacing(Facing::right); // [spawn from_first]
    play.expectRoomsEntered(2).expectLastAutosave("gallery", 200, 300, Facing::right);

    play.clickObject("door_first").settle();
    play.expectRoom("first").expectFeet(1120, 330).expectFacing(
            Facing::left); // [spawn from_gallery]
    play.expectRoomsEntered(3).expectLastAutosave("first", 1120, 330, Facing::left);

    // `pointclick --room <room> <spawn>`: the same arrival, straight from a new game.
    WalkthroughDriver direct("gallery", "from_first");
    direct.expectRoom("gallery").expectFeet(200, 300).expectFacing(Facing::right);
    direct.expectLastAutosave("gallery", 200, 300, Facing::right);
    WalkthroughDriver back("first", "from_gallery");
    back.expectFeet(1120, 330).expectFacing(Facing::left);
    // Progress is kept across the doors: a flag set before crossing is there on the other side.
    play.setFlag("crossed", 1).goTo("gallery", "from_first").settle().expectRoom("gallery");
    play.expectFlag("crossed", 1);
}

// wide (2560): the camera follows smoothly and stops at the room's edges, and a click on the
// screen lands where the displaced camera says, not at the same number in the room.
TEST_CASE("testWalkthroughWideCamera") {
    WalkthroughDriver play("wide", "west");
    play.expectRoom("wide").expectFeet(160, 450).expectFacing(Facing::right).expectCameraX(0);

    // Camera at 0: the screen x is the room x.
    play.click(900, 450).settle().expectFeet(900, 450);
    // The camera is still catching up while the player stops; it ends centered on the feet.
    play.advance(2.0F).expectCameraX(260);
    // Camera at 260: screen x 1000 is room x 1260 (not 1000).
    play.click(1000, 450).settle().expectFeet(1260, 450);
    play.advance(2.0F).expectCameraX(620);
    // And back: screen x 100 is room x 720.
    play.click(100, 450).settle().expectFeet(720, 450);

    // The east end: the camera snaps there on arrival and never shows past the room.
    WalkthroughDriver east("wide", "east");
    east.expectFeet(2400, 450).expectFacing(Facing::left).expectCameraX(1280);
    east.click(1270, 450).settle().advance(2.0F);
    east.expectCameraX(1280); // clamped: 2560 - 1280
    const SDL_FRect view = east.game().scene().camera().viewportBounds();
    REQUIRE(view.x + view.w == doctest::Approx(east.game().scene().room().width));
    // A click past the last walkable column goes to the nearest point inside the room.
    const SDL_FPoint edge = east.game().scene().room().clampDestination({2550.0F, 450.0F});
    REQUIRE(edge.x < 2550.0F);
    east.expectFeet(edge.x, edge.y);
    // An object seen with the displaced camera is clicked where it is on screen: x 2275 - 1280.
    east.clickObject("east_lamp").settle();
    east.expectLines({"A lanterna do outro lado da sala."});
    // Screen x 100 with the camera at 1280 is room x 1380.
    east.click(100, 450).settle().expectFeet(1380, 450);
}

// door: sprites and clips. wait_animation ends (no softlock), set_animation is progress that
// survives leaving the room and the save, play_animation is only of the moment.
TEST_CASE("testWalkthroughDoorAnimations") {
    WalkthroughDriver play("door", "entry");
    play.expectRoom("door").expectFeet(640, 480);
    play.expectObjectAnimation("door", "closed", true)
            .expectObjectAnimation("knocker", "idle", true);

    // flip_x is on the twin's clips and not on the first door's (pixels: SpriteTests.cpp).
    const Room& room = play.game().scene().room();
    REQUIRE(!findAnimation(room.findObject("door")->animations, "open")->flipX);
    REQUIRE(findAnimation(room.findObject("door_twin")->animations, "open")->flipX);

    play.clickObject("door").settle();
    play.expectLines({"A porta rangeu e abriu."}).expectFlag("door_open", 1);
    play.expectObjectAnimation("door", "open", true)
            .expectObjectAnimation("door_twin", "closed", true);
    play.expectFacing(Facing::left).expectFeet(360, 300); // the walk ended heading left

    // play_animation: the same clip plays again and the rule says the door was already open.
    play.clickObject("door").settle();
    play.expectLastLine("Ela já estava aberta.").expectObjectAnimation("door", "open", true);

    play.clickObject("door_twin").settle();
    play.expectLastLine("A porta gêmea abriu espelhada.")
            .expectObjectAnimation("door_twin", "open", true);

    // play_animation is of the moment: the knocker pulses and comes back to idle with the room.
    play.clickObject("knocker").settle();
    play.expectLastLine("Toc, toc.").expectObjectAnimation("knocker", "pulse", true);

    // Out and back: the doors stay open (on their last frame), the knocker starts over.
    play.goTo("first").settle().expectRoom("first");
    play.goTo("door", "entry").settle().expectRoom("door");
    play.expectObjectAnimation("door", "open", true)
            .expectObjectAnimation("door_twin", "open", true);
    play.expectObjectAnimation("knocker", "idle", true);

    // The save keeps it too (save version 3): the resumed game shows the same doors.
    const std::string saved = SaveFiles::serialize(play.game().savedGame());
    REQUIRE(saved.find("version = 3\n") != std::string::npos);
    REQUIRE(saved.find("object.door.door.animation = open\n") != std::string::npos);
    REQUIRE(saved.find("object.door.door_twin.animation = open\n") != std::string::npos);
    REQUIRE(saved.find("knocker") == std::string::npos); // play_animation is not progress
    play.saveAndResume().expectRoom("door").expectFlag("door_open", 1);
    play.expectObjectAnimation("door", "open", true)
            .expectObjectAnimation("door_twin", "open", true);
    play.expectObjectAnimation("knocker", "idle", true);
}

// lever: a long sequence (face, say, wait, set_solid, walk_to, say, wait, face, say) in
// cutscene mode: a click only skips the line, and the whole script ends without a softlock.
TEST_CASE("testWalkthroughLeverCutscene") {
    WalkthroughDriver play("lever");
    play.expectObjectSolid("gate", true);
    play.clickObject("lever").waitUntil("the lever script starts",
                                        [&] { return play.game().inCutscene(); });
    play.expectLastLine("Vou puxar a alavanca.")
            .expectFacing(Facing::left)
            .expectFlag("lever_pulled", 1);

    // In a cutscene a click on the floor does not walk anywhere: it only skips the line.
    const SDL_FPoint stopped = play.game().scene().player().feet();
    REQUIRE(play.game().scene().floatingText().isVisible());
    play.click(900, 650);
    REQUIRE(!play.game().scene().floatingText().isVisible());
    REQUIRE(!play.game().scene().player().moving());
    play.expectFeet(stopped.x, stopped.y, 0.01F);

    play.settle(); // wait 0.5, set_solid, walk_to gate, face up, say, wait 0.5, face right, say
    play.expectLines({"Vou puxar a alavanca.", "O portão abriu.", "Pronto."});
    play.expectObjectSolid("gate", false).expectFacing(Facing::right);
    play.expectFeet(970, 510); // the gate's approach point, now that it no longer blocks
    REQUIRE(play.game().scene().navigation().canStandAt({970.0F, 510.0F}));

    // After it: the plain rules.
    play.clickObject("lever").settle().expectLastLine("Já puxei a alavanca.");
    play.clickObject("gate").settle().expectLastLine("O portão está aberto.");
}

// talk: a conversation with once, goto, conditions and a change_room inside it.
TEST_CASE("testWalkthroughTalkDialogue") {
    WalkthroughDriver play("talk");
    play.clickObject("guide").waitForChoice();
    play.expectLines({"Olá, viajante. Posso ajudar?"});
    play.expectChoices({"nome", "sair"}); // mapa needs knows_guide, seguir needs the map

    play.choose("nome").waitForChoice();
    play.expectFlag("knows_guide", 1);
    play.expectChoices({"mapa", "sair"}); // `nome` was once; `mapa` now has its condition
    REQUIRE(play.game().session().dialogueChoiceTaken("talk", "guide", "nome"));

    play.choose("mapa").waitForChoice();
    play.expectItems({"guide_map"});
    play.expectChoices({"seguir", "sair"}); // lacks_item hides `mapa`; has_item shows `seguir`

    // The room only changes when the conversation is over.
    play.choose("seguir");
    play.expectRoom("talk");
    play.settle();
    play.expectRoom("items").expectFeet(160, 560).expectFacing(Facing::right);
    play.expectLines({"Olá, viajante. Posso ajudar?", "Sou só um guia.",
                      "Olá, viajante. Posso ajudar?", "Aqui está o mapa.",
                      "Olá, viajante. Posso ajudar?", "Venha comigo."});
    play.expectLastAutosave("items", 160, 560, Facing::right);
    play.expectItems({"guide_map"}).expectFlag("knows_guide", 1);

    // The once choice is progress: it survives the save and the menu still hides it.
    WalkthroughDriver again("talk");
    again.clickObject("guide").waitForChoice().choose("nome").waitForChoice();
    again.choose("sair").settle().saveAndResume().expectRoom("talk");
    again.clickObject("guide").waitForChoice().expectChoices({"mapa", "sair"});
    again.choose("sair").settle().expectLastLine("Até logo.").expectRoom("talk");
}

// items: pick up, use an item on an object, the wildcard rules in their fixed order and the
// counters (with the limit that saturates instead of overflowing).
TEST_CASE("testWalkthroughItemsWildcardsAndCounters") {
    WalkthroughDriver play("items");
    // Nothing held and no rule for the statue: the [interact *] answer.
    play.clickObject("statue").settle().expectLastLine("Nada de especial.");

    play.clickObject("key_crate").settle();
    play.expectItems({"brass_key"}).expectObjectVisible("key_crate", false);
    play.expectLastLine("Peguei a chave de latão.");
    play.clickObject("feather_stand").settle();
    play.expectItems({"brass_key", "feather"}).expectObjectVisible("feather_stand", false);

    // The wildcard rules come first in the file, yet the order is by specificity:
    play.use("feather", "red_box")
            .settle()
            .expectLastLine("A caixa não abre com isso."); // * red_box
    play.use("brass_key", "statue")
            .settle()
            .expectLastLine("A chave não serve aqui.");                          // brass_key *
    play.use("feather", "statue").settle().expectLastLine("Isso não funciona."); // * *
    play.expectItems({"brass_key", "feather"}).expectObjectSolid("red_box", true);
    // The exact rule beats all three, wherever it is written.
    play.use("brass_key", "red_box").settle().expectLastLine("A chave abriu a caixa.");
    play.expectFlag("red_box_unlocked", 1)
            .expectObjectSolid("red_box", false)
            .expectItems({"feather"});
    // An exact interact rule whose condition fails falls to the next level; a passing one answers.
    play.clickObject("red_box").settle().expectLastLine("A caixa está aberta.");
    play.use("feather", "red_box").settle().expectLastLine("A caixa não abre com isso.");

    // Counters: the pile fills up to 3 (the first rule takes over from there), the well empties it.
    for (int coin = 1; coin <= 3; ++coin) {
        play.clickObject("coin_pile")
                .settle()
                .expectLastLine("Mais uma moeda.")
                .expectFlag("coins", coin);
    }
    play.clickObject("coin_pile").settle().expectLastLine("Já peguei todas as moedas.");
    play.expectFlag("coins", 3);
    for (int coin = 2; coin >= 0; --coin) {
        play.clickObject("well").settle().expectLastLine("Joguei uma moeda no poço.");
        play.expectFlag("coins", coin);
    }
    REQUIRE(!play.game().session().hasFlag("coins")); // reaching 0 clears the flag
    play.clickObject("well").settle().expectLastLine("Sem moedas para jogar.");

    // The limit: the vault adds 1.000.000 per click and the counter stops at 1.000.000.000.
    play.setFlag("treasure", 999'500'000);
    play.clickObject("vault").settle().expectLastLine("Guardei mais um milhão.");
    play.expectFlag("treasure", kMaxFlagValue);
    play.clickObject("vault").settle().expectFlag("treasure", kMaxFlagValue);
    // Saturated progress is still a valid save, and comes back as it was.
    play.saveAndResume().expectRoom("items").expectFlag("treasure", kMaxFlagValue);
    play.expectItems({"feather"}).expectObjectVisible("key_crate", false);
    play.expectObjectSolid("red_box", false);
}

// The driver's own promises: the same steps always end in the same state at the same simulated
// time (no real clock), and a failed step says which step, what was obtained against what was
// expected, and the state.
TEST_CASE("testWalkthroughIsDeterministic") {
    const auto played = [] {
        WalkthroughDriver play("lever");
        play.clickObject("lever").settle();
        return std::pair{play.describeState(), play.seconds()};
    };
    const auto first = played();
    const auto second = played();
    REQUIRE(first.first == second.first);
    REQUIRE(first.second == second.second);
    REQUIRE(first.second > 3.0); // the script really waited: seconds of simulated time
}

TEST_CASE("testWalkthroughReportsFailures") {
    const auto failureOf = [](const auto& step) {
        try {
            step();
        } catch (const WalkthroughFailure& failure) {
            return std::string(failure.what());
        }
        return std::string();
    };
    const auto mentions = [](const std::string& message, const std::string_view text) {
        INFO(message);
        return message.find(text) != std::string::npos;
    };

    WalkthroughDriver play("items");
    // An expectation names its step and shows obtained against expected, with the state.
    const std::string wrongRoom = failureOf([&] { play.expectRoom("lever"); });
    REQUIRE(mentions(wrongRoom, "passo 2) expect room lever"));
    REQUIRE(mentions(wrongRoom, "sala: esperado \"lever\", obtido \"items\""));
    REQUIRE(mentions(wrongRoom, "estado: sala=items"));
    REQUIRE(mentions(wrongRoom, "1) start new game in items")); // the steps before it
    REQUIRE(mentions(failureOf([&] { play.expectFlag("coins", 4); }),
                     "flag coins: esperado 4, obtido 0"));
    REQUIRE(mentions(failureOf([&] { play.expectItems({"brass_key"}); }),
                     "inventario: esperado [\"brass_key\"], obtido []"));
    // A click or an item the game refuses is a failure of that step, not a silent no-op.
    REQUIRE(mentions(failureOf([&] { play.use("brass_key", "red_box"); }), "o jogo recusou o uso"));
    REQUIRE(mentions(failureOf([&] { play.choose("nome"); }), "nao ha menu de escolhas"));
    REQUIRE(mentions(failureOf([&] { play.clickObject("nada"); }), "a sala nao tem o objeto nada"));
    // A wait that cannot end is a softlock report, with the state it stopped in.
    WalkthroughDriver lever("lever");
    lever.clickObject("lever");
    const std::string stuck = failureOf([&] { lever.settle(1.0F); });
    REQUIRE(mentions(stuck, "continuou ocupado depois de 1 s simulados (softlock?)"));
    REQUIRE(mentions(stuck, "estado: sala=lever"));
    REQUIRE(mentions(stuck, "andando=sim")); // still walking to the lever when the limit hit
    REQUIRE(mentions(failureOf([&] { lever.waitUntil("never", [] { return false; }, 0.5F); }),
                     "a condicao 'never' nao valeu em 0.5 s"));
    // A conversation that waits for a choice says so instead of timing out.
    WalkthroughDriver talk("talk");
    talk.clickObject("guide").waitForChoice();
    REQUIRE(mentions(failureOf([&] { talk.settle(); }),
                     "a conversa espera uma escolha [\"nome\", \"sair\"]"));
    REQUIRE(mentions(failureOf([&] { talk.choose("mapa"); }), "a escolha mapa nao esta no menu"));
}
