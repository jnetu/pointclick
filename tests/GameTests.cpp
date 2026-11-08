#include "support/RoomPresets.hpp"
#include "game/Camera.hpp"
#include "game/Game.hpp"
#include "game/Navigation.hpp"

#include <cassert>
#include <vector>
#include <variant>
#include <string>
#include <optional>
#include <cmath>

namespace {
// The last room change a finished script asked for, if not drained yet.
std::optional<std::string> requestedRoom(const Game& game) {
    std::optional<std::string> last;
    for (const GameEvent& event : game.pendingEvents()) {
        if (const auto* change = std::get_if<RoomChangeRequested>(&event)) last = change->roomId;
    }
    return last;
}

std::optional<std::string> consumeRequestedRoom(Game& game) {
    std::optional<std::string> requested = requestedRoom(game);
    std::vector<GameEvent> drained;
    game.drainEvents(drained);
    return requested;
}
bool near(const float a, const float b) { return std::abs(a - b) < 0.01F; }

void testMovementAndRetargeting() {
    Game game(RoomPresets::firstRoom());
    assert(near(game.scene().player().feet().x, 640.0F));
    assert(near(game.scene().player().feet().y, 360.0F));

    game.onClick({900.0F, 360.0F});
    game.tick(0.5F);
    assert(near(game.scene().player().feet().x, 640.0F + game.scene().room().speedAt(360.0F) * 0.5F));
    game.onClick({750.0F, 500.0F});
    for (int i = 0; i < 20; ++i) {
        game.tick(0.05F);
    }
    assert(near(game.scene().player().feet().x, 750.0F));
    assert(near(game.scene().player().feet().y, 500.0F));
    assert(!game.scene().player().moving());

    game.onClick({-10.0F, 200.0F});
    assert(!game.scene().player().moving());
    game.onClick({2.0F, 2.0F});
    assert(near(game.scene().player().destination().x, 750.0F));
    assert(near(game.scene().player().destination().y, 500.0F));
    game.onClick({2.0F, game.scene().room().wallBottomY + 1.0F});
    const float expectedY = game.scene().room().walkableTopY;
    assert(near(game.scene().player().destination().y, expectedY));
    assert(near(game.scene().player().destination().x,
                game.scene().room().playerBaseSize.x * game.scene().room().scaleAt(expectedY) * 0.5F));
}

void testRoomPerspective() {
    Room room = RoomPresets::firstRoom();
    assert(near(room.scaleAt(room.farDepthY), room.farScale));
    assert(near(room.scaleAt(room.nearDepthY), room.nearScale));
    assert(room.scaleAt(360.0F) < room.scaleAt(600.0F));
    assert(near(room.speedAt(room.farDepthY), room.playerFarSpeed));
    assert(near(room.speedAt(room.nearDepthY), room.playerNearSpeed));
    assert(room.speedAt(360.0F) < room.speedAt(600.0F));
    assert(room.playerBounds({640.0F, 360.0F}).w
           < room.playerBounds({640.0F, 600.0F}).w);

    room.wallBottomY = 350.0F;
    room.walkableTopY = 370.0F;
    room.farDepthY = 370.0F;
    room.farScale = 0.4F;
    room.nearScale = 2.0F;
    room.playerFarSpeed = 100.0F;
    room.playerNearSpeed = 100.0F;
    room.playerStart = {640.0F, 400.0F};
    Game custom(room);
    custom.onClick({800.0F, 320.0F});
    assert(!custom.scene().player().moving());
    custom.onClick({800.0F, 400.0F});
    custom.tick(0.5F);
    assert(custom.scene().player().moving());
    assert(near(std::hypot(custom.scene().player().feet().x - room.playerStart.x,
                           custom.scene().player().feet().y - room.playerStart.y), 50.0F));

    Room nextRoom = RoomPresets::firstRoom();
    nextRoom.playerStart = {300.0F, 620.0F};
    custom.loadRoom(nextRoom);
    assert(near(custom.scene().player().feet().x, 300.0F));
    assert(near(custom.scene().player().feet().y, 620.0F));
    assert(!custom.scene().player().moving());
}

void testPerspectiveMovementSpeed() {
    Room room = RoomPresets::firstRoom();
    room.scenery.clear();
    room.playerStart = {500.0F, 320.0F};
    Game far(room);
    far.onClick({1000.0F, 320.0F});
    far.tick(0.5F);
    const float farDistance = far.scene().player().feet().x - 500.0F;

    room.playerStart = {500.0F, 650.0F};
    Game nearGame(room);
    nearGame.onClick({1000.0F, 650.0F});
    nearGame.tick(0.5F);
    const float nearDistance = nearGame.scene().player().feet().x - 500.0F;
    assert(nearDistance > farDistance);
    assert(near(farDistance, room.speedAt(320.0F) * 0.5F));
    assert(near(nearDistance, room.speedAt(650.0F) * 0.5F));
}

void testObstacleRoute() {
    Navigation corner;
    corner.setBlocked(1, 1, true);
    const auto aroundCorner = corner.findPath({31, 33}, {33, 31});
    assert(aroundCorner.size() > 1);
    assert(corner.findPath({10, 10}, {10, 720}).empty());
    Navigation navigation;
    for (int row = 8; row <= 13; ++row) {
        navigation.setBlocked(19, row, true);
    }
    const SDL_FPoint start{500.0F, 360.0F};
    const SDL_FPoint goal{750.0F, 360.0F};
    const auto route = navigation.findPath(start, goal);
    assert(route.size() > 1);
    assert(near(route.back().x, goal.x) && near(route.back().y, goal.y));

    SDL_FPoint from = start;
    for (const SDL_FPoint to : route) {
        const float length = std::hypot(to.x - from.x, to.y - from.y);
        const int steps = static_cast<int>(std::ceil(length));
        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const int column = static_cast<int>((from.x + (to.x - from.x) * t) / Navigation::cellSize);
            const int row = static_cast<int>((from.y + (to.y - from.y) * t) / Navigation::cellSize);
            assert(column != 19 || row < 8 || row > 13);
        }
        from = to;
    }
    navigation.setBlocked(23, 11, true);
    assert(navigation.findPath(start, goal).empty());

    Navigation floorOnly;
    floorOnly.setWalkableTop(300.0F);
    for (int row = 9; row <= 14; ++row) {
        floorOnly.setBlocked(19, row, true);
    }
    const auto floorRoute = floorOnly.findPath(start, goal);
    assert(!floorRoute.empty());
    for (const SDL_FPoint waypoint : floorRoute) {
        assert(waypoint.y >= 300.0F);
    }
}

void testSolidObjectFootprint() {
    Room room = RoomPresets::firstRoom();
    assert(room.scenery[0].type == SceneObjectType::decoration);
    assert(room.scenery[1].type == SceneObjectType::solid);
    const SceneObject& red = room.scenery[1];
    const SDL_FRect footprint = red.worldCollisionFootprint();
    assert(footprint.w < red.bounds.w && footprint.h < red.bounds.h);

    Game game(room);
    game.onClick({860.0F, 500.0F}); // The upper visual area is passable.
    assert(near(game.scene().player().destination().x, 860.0F));
    assert(near(game.scene().player().destination().y, 500.0F));
    game.onClick({860.0F, 575.0F}); // The solid base snaps to reachable ground.
    const SDL_FPoint snapped = game.scene().player().destination();
    assert(!SDL_PointInRectFloat(&snapped, &footprint));

    room.playerStart = {730.0F, 575.0F};
    Game detour(room);
    detour.onClick({1050.0F, 575.0F});
    assert(detour.scene().player().remainingWaypoints() > 1);
    for (int i = 0; i < 100; ++i) {
        detour.tick(0.05F);
        const SDL_FPoint feet = detour.scene().player().feet();
        assert(!SDL_PointInRectFloat(&feet, &footprint));
    }
    assert(!detour.scene().player().moving());
    assert(near(detour.scene().player().feet().x, 1050.0F));
    assert(near(detour.scene().player().feet().y, 575.0F));
}

void testStandardRoomCameraFixed() {
    Game game(RoomPresets::firstRoom());
    assert(game.scene().navigation().columns() == 40);
    assert(game.scene().navigation().rows() == 23);
    assert(near(game.scene().camera().position().x, 0.0F));
    assert(near(game.scene().camera().position().y, 0.0F));

    const SDL_FPoint targets[]{{80.0F, 340.0F}, {1220.0F, 680.0F}, {640.0F, 500.0F}};
    for (const SDL_FPoint target : targets) {
        game.onClick(target);
        for (int i = 0; i < 60; ++i) {
            game.tick(0.05F);
            for (const float alpha : {0.0F, 0.5F, 1.0F}) {
                const SDL_FPoint cam = game.scene().camera().interpolatedPosition(alpha);
                assert(near(cam.x, 0.0F) && near(cam.y, 0.0F));
            }
        }
    }
}

void testPanoramicRoomCameraAndNavigation() {
    Room room = RoomPresets::firstRoom();
    room.id = "panorama";
    room.width = 2560.0F;
    room.height = 720.0F;
    room.vanishingPointX = 1280.0F;
    room.playerStart = {200.0F, 560.0F};
    room.scenery = {
        {{1750.0F, 450.0F, 120.0F, 130.0F}, {220, 74, 68, 255}, SceneObjectType::solid,
         {10.0F, 95.0F, 100.0F, 35.0F}, "east_pillar", {}, "idle"},
    };
    const SDL_FRect eastFootprint = room.scenery[0].worldCollisionFootprint();

    Game game(room);
    assert(game.scene().navigation().columns() == 80);
    assert(game.scene().navigation().rows() == 23);
    // Spawn near the left border clamps the initial camera to x = 0.
    assert(near(game.scene().camera().position().x, 0.0F));
    assert(near(game.scene().camera().position().y, 0.0F));

    game.onClick({2350.0F, 560.0F});
    assert(game.scene().player().moving());
    assert(game.scene().player().remainingWaypoints() > 1);

    bool sawSmoothLag = false;
    bool crossedOriginalWidth = false;
    float previousCameraX = game.scene().camera().position().x;
    for (int i = 0; i < 220; ++i) {
        const float beforeTickCamX = game.scene().camera().position().x;
        game.tick(0.05F);
        const SDL_FPoint feet = game.scene().player().feet();
        const SDL_FPoint cam = game.scene().camera().position();
        const SDL_FPoint mid = game.scene().camera().interpolatedPosition(0.5F);
        assert(!SDL_PointInRectFloat(&feet, &eastFootprint));
        assert(cam.x >= 0.0F && cam.x <= room.width - World::width);
        assert(near(cam.y, 0.0F));
        assert(cam.x + 0.001F >= previousCameraX);
        assert(mid.x >= beforeTickCamX - 0.001F && mid.x <= cam.x + 0.001F);
        if (feet.x > 900.0F && feet.x < 1600.0F && game.scene().player().moving()) {
            const float idealTargetX = game.scene().camera().targetFor(feet).x;
            if (cam.x > 0.0F && cam.x + 1.0F < idealTargetX) {
                sawSmoothLag = true;
            }
        }
        if (feet.x > 1280.0F) {
            crossedOriginalWidth = true;
        }
        previousCameraX = cam.x;
    }
    assert(crossedOriginalWidth);
    assert(sawSmoothLag);
    assert(!game.scene().player().moving());
    assert(near(game.scene().player().feet().x, 2350.0F));
    // Near the right border (2350 - 640 = 1710 > 1280), the camera clamps at 1280.
    assert(near(game.scene().camera().position().x, 1280.0F));

    // Clicking inside the solid footprint at x > 1280 snaps to nearest walkable ground.
    game.onClick({1800.0F, 565.0F});
    const SDL_FPoint snapped = game.scene().player().destination();
    assert(!SDL_PointInRectFloat(&snapped, &eastFootprint));

    // Teleporting snaps the camera immediately with zero lag.
    std::string error;
    assert(game.teleportPlayer({1400.0F, 520.0F}, error));
    assert(near(game.scene().camera().position().x, 760.0F));
    assert(near(game.scene().camera().interpolatedPosition(0.0F).x, 760.0F));

    // Live editor tweaks (resetPlayer == false) keep continuity and clamp when shrinking.
    Room faster = game.scene().room();
    faster.playerFarSpeed = 160.0F;
    assert(game.applyRoom(faster, false, error));
    assert(near(game.scene().camera().position().x, 760.0F));

    Room narrower = game.scene().room();
    narrower.scenery.clear();
    narrower.width = 1800.0F;
    assert(game.applyRoom(narrower, false, error));
    assert(near(game.scene().camera().position().x, 520.0F));

    // Loading a room strong-snaps both X and Y camera axes to the spawn target.
    Room tallAndWide = RoomPresets::firstRoom();
    tallAndWide.scenery.clear();
    tallAndWide.width = 2560.0F;
    tallAndWide.height = 1440.0F;
    tallAndWide.nearDepthY = 1440.0F;
    tallAndWide.playerStart = {1900.0F, 1000.0F};
    game.loadRoom(tallAndWide);
    assert(near(game.scene().camera().position().x, 1260.0F));
    assert(near(game.scene().camera().position().y, 640.0F));
}

void testWalkToInteractAndDepthPicking() {
    Room room = RoomPresets::firstRoom();
    room.playerStart = {220.0F, 520.0F};
    // Overlap two interactive objects at x = 700..800, y = 430..500:
    // back_statue has depth 510; front_banner has depth 550 (drawn in front).
    room.scenery = {
        {{680.0F, 390.0F, 120.0F, 120.0F}, {120, 120, 160, 255}, SceneObjectType::decoration,
         {}, "back_statue", {}, "idle"},
        {{700.0F, 430.0F, 110.0F, 120.0F}, {200, 160, 80, 255}, SceneObjectType::decoration,
         {}, "front_banner", {}, "idle"},
        // Painting mounted high on the wall (above wallBottomY = 280) must also be clickable.
        {{420.0F, 120.0F, 90.0F, 110.0F}, {180, 140, 110, 255}, SceneObjectType::decoration,
         {}, "wall_painting", {}, "idle"},
    };

    Game game(room);
    game.setInteractions(RoomInteractions{{
        {
            .trigger = InteractionTrigger::interact("back_statue"),
            .conditions = {},
            .effects = {
                InteractionEffect::make("set_flag", {"inspected_statue"}),
                InteractionEffect::make("say", {"Uma estatua antiga."}),
            },
        },
        {
            .trigger = InteractionTrigger::interact("front_banner"),
            .conditions = {InteractionCondition::make("flag_not_set", {"took_banner"})},
            .effects = {
                InteractionEffect::make("set_flag", {"took_banner"}),
                InteractionEffect::make("hide_object", {"front_banner"}),
                InteractionEffect::make("say", {"Retirei o estandarte da frente."}),
            },
        },
        {
            .trigger = InteractionTrigger::interact("wall_painting"),
            .conditions = {},
            .effects = {
                InteractionEffect::make("set_flag", {"saw_painting"}),
                InteractionEffect::make("say", {"Um retrato na parede."}),
            },
        },
    }});

    // 1. Click in the overlapping region (740, 470): must pick front_banner (higher depth),
    // start walking, and NOT trigger the effect before arrival.
    game.onClick({740.0F, 470.0F});
    assert(game.scene().player().moving());
    assert(game.scene().pendingInteraction().has_value());
    assert(game.scene().pendingInteraction()->objectId == "front_banner");
    assert(!game.session().hasFlag("took_banner"));
    assert(!game.scene().floatingText().isVisible());

    game.tick(0.25F);
    assert(game.scene().player().moving());
    assert(!game.session().hasFlag("took_banner"));

    // 2. Clicking empty floor mid-walk cancels the pending interaction and reroutes.
    game.onClick({320.0F, 600.0F});
    assert(!game.scene().pendingInteraction().has_value());
    for (int i = 0; i < 80; ++i) {
        game.tick(0.05F);
    }
    assert(!game.scene().player().moving());
    assert(near(game.scene().player().feet().x, 320.0F) && near(game.scene().player().feet().y, 600.0F));
    assert(!game.session().hasFlag("took_banner"));
    assert(game.scene().isObjectVisible("front_banner"));

    // 3. Click the overlapping spot again and walk all the way to arrival: front_banner fires and hides.
    game.onClick({740.0F, 470.0F});
    assert(game.scene().pendingInteraction().has_value());
    assert(game.scene().pendingInteraction()->objectId == "front_banner");
    for (int i = 0; i < 100; ++i) {
        game.tick(0.05F);
    }
    assert(!game.scene().player().moving());
    assert(!game.scene().pendingInteraction().has_value());
    assert(game.session().hasFlag("took_banner"));
    assert(!game.scene().isObjectVisible("front_banner"));
    assert(game.scene().floatingText().text() == "Retirei o estandarte da frente.");
    assert(!game.session().hasFlag("inspected_statue"));

    // 4. Clicking the exact same spot (740, 470) now ignores hidden front_banner and picks back_statue.
    game.onClick({740.0F, 470.0F});
    for (int i = 0; i < 60; ++i) {
        game.tick(0.05F);
    }
    assert(!game.scene().player().moving());
    assert(game.session().hasFlag("inspected_statue"));
    assert(game.scene().floatingText().text() == "Uma estatua antiga.");

    // 5. Clicking back_statue while ALREADY standing at its interaction point fires immediately.
    game.session().clearFlag("inspected_statue");
    game.onClick({740.0F, 470.0F});
    assert(!game.scene().player().moving());
    assert(!game.scene().pendingInteraction().has_value());
    assert(game.session().hasFlag("inspected_statue"));

    // 6. Clicking wall_painting above wallBottomY walks to walkableTopY beneath it and interacts.
    game.onClick({465.0F, 175.0F});
    assert(game.scene().player().moving());
    for (int i = 0; i < 80; ++i) {
        game.tick(0.05F);
    }
    assert(!game.scene().player().moving());
    assert(near(game.scene().player().feet().y, room.walkableTopY));
    assert(game.session().hasFlag("saw_painting"));
    assert(game.scene().floatingText().text() == "Um retrato na parede.");
}

void testItemPickupAndHiddenCollision() {
    Room room = RoomPresets::firstRoom();
    room.playerStart = {250.0F, 520.0F};
    // Solid crate with collectible behavior: picking it up adds to inventory, hides it,
    // and frees its blocked navigation cells.
    room.scenery = {
        {{580.0F, 470.0F, 90.0F, 90.0F}, {210, 175, 65, 255}, SceneObjectType::solid,
         {10.0F, 60.0F, 70.0F, 30.0F}, "supply_crate", {}, "idle"},
    };
    const SDL_FPoint crateCenter{625.0F, 545.0F};

    Game game(room);
    assert(!game.scene().navigation().canStandAt(crateCenter));

    game.setInteractions(RoomInteractions{{
        {
            .trigger = InteractionTrigger::interact("supply_crate"),
            .conditions = {InteractionCondition::make("object_visible", {"supply_crate"})},
            .effects = {
                InteractionEffect::make("give_item", {"crowbar"}),
                InteractionEffect::make("hide_object", {"supply_crate"}),
                InteractionEffect::make("say", {"Peguei o pe de cabra e recolhi a caixa."}),
            },
        },
    }});

    game.onClick({625.0F, 515.0F});
    assert(game.scene().player().moving());
    assert(!game.session().hasItem("crowbar"));
    for (int i = 0; i < 80; ++i) {
        game.tick(0.05F);
    }
    assert(!game.scene().player().moving());
    assert(game.session().hasItem("crowbar"));
    assert(game.session().items().size() == 1);
    assert(game.session().items()[0] == "crowbar");
    assert(!game.scene().isObjectVisible("supply_crate"));
    assert(!game.scene().isObjectSolid(game.scene().room().scenery[0]));
    // Hidden solid object no longer blocks navigation, and clicking its old bounds walks onto the floor.
    assert(game.scene().navigation().canStandAt(crateCenter));

    game.onClick(crateCenter);
    assert(!game.scene().pendingInteraction().has_value());
    for (int i = 0; i < 40; ++i) {
        game.tick(0.05F);
    }
    assert(near(game.scene().player().feet().x, crateCenter.x));
    assert(near(game.scene().player().feet().y, crateCenter.y));
    assert(!game.scene().floatingText().isVisible());
}

void testConditionalNpcPuzzleAndRoomPersistence() {
    Room room = RoomPresets::firstRoom();
    room.id = "courtyard";
    room.playerStart = {200.0F, 520.0F};
    SpriteClip closedClip{.file = "door_closed.png"};
    SpriteClip openClip{.file = "door_open.png"};
    AnimationSet doorAnims{{"idle", closedClip}, {"open", openClip}};

    // Gate spans the entire walkable height at x = 700, partitioning the room until unlocked!
    room.scenery = {
        {{360.0F, 470.0F, 60.0F, 50.0F}, {240, 210, 80, 255}, SceneObjectType::decoration,
         {}, "silver_key", {}, "idle"},
        {{500.0F, 360.0F, 70.0F, 140.0F}, {90, 140, 200, 255}, SceneObjectType::solid,
         {10.0F, 110.0F, 50.0F, 30.0F}, "guard", {}, "idle"},
        {{700.0F, 280.0F, 80.0F, 440.0F}, {140, 95, 65, 255}, SceneObjectType::solid,
         {10.0F, 0.0F, 60.0F, 440.0F}, "iron_gate", doorAnims, "idle"},
        {{1020.0F, 480.0F, 70.0F, 70.0F}, {90, 220, 180, 255}, SceneObjectType::decoration,
         {}, "relic", {}, "idle"},
    };

    Game game(room);
    game.setInteractions(RoomInteractions{{
        {
            .trigger = InteractionTrigger::interact("silver_key"),
            .conditions = {InteractionCondition::make("object_visible", {"silver_key"})},
            .effects = {
                InteractionEffect::make("give_item", {"silver_key"}),
                InteractionEffect::make("hide_object", {"silver_key"}),
                InteractionEffect::make("say", {"Peguei a chave prateada."}),
            },
        },
        {
            .trigger = InteractionTrigger::interact("guard"),
            .conditions = {InteractionCondition::make("flag_set", {"gate_open"})},
            .effects = {InteractionEffect::make("say", {"O portao esta aberto. Boa sorte la dentro."})},
        },
        {
            .trigger = InteractionTrigger::interact("guard"),
            .conditions = {
                InteractionCondition::make("has_item", {"silver_key"}),
                InteractionCondition::make("flag_set", {"talked_to_guard"}),
            },
            .effects = {
                InteractionEffect::make("take_item", {"silver_key"}),
                InteractionEffect::make("set_flag", {"gate_open"}),
                InteractionEffect::make("set_solid", {"iron_gate", "0"}),
                InteractionEffect::make("play_animation", {"iron_gate", "open"}),
                InteractionEffect::make("say", {"Essa e a chave! Vou abrir o portao."}),
            },
        },
        {
            .trigger = InteractionTrigger::useItem("silver_key", "iron_gate"),
            .conditions = {InteractionCondition::make("flag_not_set", {"gate_open"})},
            .effects = {
                InteractionEffect::make("take_item", {"silver_key"}),
                InteractionEffect::make("set_flag", {"gate_open"}),
                InteractionEffect::make("set_solid", {"iron_gate", "0"}),
                InteractionEffect::make("play_animation", {"iron_gate", "open"}),
                InteractionEffect::make("say", {"Abri o portao com a chave."}),
            },
        },
        {
            .trigger = InteractionTrigger::interact("guard"),
            .conditions = {InteractionCondition::make("lacks_item", {"silver_key"})},
            .effects = {
                InteractionEffect::make("set_flag", {"talked_to_guard"}),
                InteractionEffect::make("say", {"Sem a chave prateada ninguem passa."}),
            },
        },
        {
            .trigger = InteractionTrigger::interact("relic"),
            .conditions = {},
            .effects = {
                InteractionEffect::make("give_item", {"relic"}),
                InteractionEffect::make("hide_object", {"relic"}),
                InteractionEffect::make("change_room", {"sanctuary"}),
                InteractionEffect::make("say", {"Encontrei a reliquia!"}),
            },
        },
    }});

    // 1. Before unlocking iron_gate, clicking relic behind the gate walks only up to the gate
    // and MUST NOT collect relic through the solid wall.
    game.onClick({1055.0F, 515.0F});
    for (int i = 0; i < 100; ++i) {
        game.tick(0.05F);
    }
    assert(game.scene().player().feet().x < 700.0F);
    assert(!game.session().hasItem("relic"));
    assert(game.scene().isObjectVisible("relic"));
    assert(!requestedRoom(game).has_value());

    // 2. Talk to guard before having silver_key: sets talked_to_guard flag, gate stays solid.
    game.onClick({535.0F, 430.0F});
    for (int i = 0; i < 80; ++i) {
        game.tick(0.05F);
    }
    assert(game.session().hasFlag("talked_to_guard"));
    assert(!game.session().hasFlag("gate_open"));
    assert(game.scene().floatingText().text() == "Sem a chave prateada ninguem passa.");
    assert(!game.scene().navigation().canStandAt({730.0F, 520.0F}));
    assert(game.scene().objectAnimation("iron_gate")->name() == "idle");

    // 3. Pick up silver_key on the floor.
    game.onClick({390.0F, 495.0F});
    for (int i = 0; i < 80; ++i) {
        game.tick(0.05F);
    }
    assert(game.session().hasItem("silver_key"));
    assert(!game.scene().isObjectVisible("silver_key"));

    // 4. Talk to guard again now that we have silver_key AND talked_to_guard:
    // consumes silver_key, sets gate_open, switches iron_gate animation to "open",
    // and rebuilds Navigation so iron_gate is passable!
    game.onClick({535.0F, 430.0F});
    for (int i = 0; i < 80; ++i) {
        game.tick(0.05F);
    }
    assert(!game.session().hasItem("silver_key"));
    assert(game.session().hasFlag("gate_open"));
    assert(game.scene().floatingText().text() == "Essa e a chave! Vou abrir o portao.");
    assert(game.scene().objectAnimation("iron_gate")->name() == "open");
    assert(game.scene().navigation().canStandAt({730.0F, 520.0F}));

    // Talking to guard a third time (already standing next to him) reflects the new state.
    game.onClick({535.0F, 430.0F});
    assert(game.scene().floatingText().text() == "O portao esta aberto. Boa sorte la dentro.");

    // 5. Walk through the unlocked gate to collect relic and trigger change_room "sanctuary".
    game.onClick({1055.0F, 515.0F});
    for (int i = 0; i < 120; ++i) {
        game.tick(0.05F);
    }
    assert(game.scene().player().feet().x > 950.0F);
    assert(game.session().hasItem("relic"));
    assert(!game.scene().isObjectVisible("relic"));
    assert(requestedRoom(game).has_value());
    assert(*requestedRoom(game) == "sanctuary");
    assert(*consumeRequestedRoom(game) == "sanctuary");
    assert(!requestedRoom(game).has_value());

    // 6. Transitioning to another room and returning preserves SessionState (inventory, flags,
    // hidden items, and unlocked iron_gate).
    Room sanctuary = RoomPresets::firstRoom();
    sanctuary.id = "sanctuary";
    game.loadRoom(sanctuary);
    assert(game.session().hasItem("relic"));
    assert(game.session().hasFlag("gate_open"));

    game.loadRoom(room);
    assert(!game.scene().isObjectVisible("silver_key"));
    assert(!game.scene().isObjectVisible("relic"));
    assert(game.scene().navigation().canStandAt({730.0F, 520.0F}));
}
}

// walk_to -> say -> play_animation -> wait_animation -> wait -> say -> change_room runs over
// several ticks; while it holds the scene, clicks only skip the current line.
void testScriptedSequenceAndCutscene() {
    Room room = RoomPresets::firstRoom();
    room.scenery[1].animations["idle"] = SpriteClip{.file = "red_idle.png"};
    room.scenery[1].animations["open"] = SpriteClip{.file = "red_open.png", .frameWidth = 16,
                                                    .frameHeight = 16, .frames = 4,
                                                    .columns = 4, .fps = 8.0F, .loop = false};
    Game game(room);
    game.setInteractions(RoomInteractions{{{
        .trigger = InteractionTrigger::interact("blue_box"),
        .conditions = {},
        .effects = {
            InteractionEffect::make("walk_to", {"red_box"}),
            InteractionEffect::make("say", {"A"}),
            InteractionEffect::make("play_animation", {"red_box", "open"}),
            InteractionEffect::make("wait_animation", {"red_box"}),
            InteractionEffect::make("wait", {"0.5"}),
            InteractionEffect::make("say", {"B"}),
            InteractionEffect::make("set_flag", {"cutscene_done"}),
            InteractionEffect::make("change_room", {"gallery"}),
        },
    }}});

    game.onClick({430.0F, 410.0F});
    for (int i = 0; i < 200 && !game.inCutscene(); ++i) game.tick(0.05F);
    assert(game.inCutscene());
    assert(game.scene().player().moving()); // walk_to red_box holds the queue

    // World clicks do not redirect the scripted walk.
    const SDL_FPoint destination = game.scene().player().destination();
    game.onClick({200.0F, 600.0F});
    assert(game.scene().player().destination().x == destination.x);
    assert(game.scene().player().destination().y == destination.y);
    assert(!game.useItemOnObject("anything", "red_box"));

    for (int i = 0; i < 300 && game.scene().player().moving(); ++i) game.tick(0.05F);
    assert(game.scene().floatingText().isVisible() && game.scene().floatingText().text() == "A");
    assert(game.inCutscene());
    assert(game.scene().objectAnimation("red_box")->name() == "idle");

    // A click skips the line; the next tick starts the animation and waits for it.
    game.onClick({200.0F, 600.0F});
    assert(!game.scene().floatingText().isVisible());
    assert(!game.scene().player().moving());
    game.tick(0.05F);
    assert(game.scene().objectAnimation("red_box")->name() == "open");
    assert(game.inCutscene());

    for (int i = 0; i < 12; ++i) game.tick(0.05F); // clip (0.5 s) ends, wait 0.5 starts
    assert(game.inCutscene());
    assert(game.scene().floatingText().text() == "A");
    for (int i = 0; i < 12; ++i) game.tick(0.05F); // wait ends, say B holds the queue
    assert(game.scene().floatingText().text() == "B");
    assert(game.inCutscene());
    assert(!game.session().hasFlag("cutscene_done"));
    assert(!requestedRoom(game).has_value());

    // B expires on its own (reading time); the rest runs and the room change is published.
    for (int i = 0; i < 400 && game.inCutscene(); ++i) game.tick(0.05F);
    assert(!game.inCutscene());
    assert(game.session().hasFlag("cutscene_done"));
    assert(requestedRoom(game) && *requestedRoom(game) == "gallery");

    // Changing room cancels any running sequence.
    game.onClick({430.0F, 410.0F});
    for (int i = 0; i < 200 && !game.inCutscene(); ++i) game.tick(0.05F);
    assert(game.inCutscene());
    Room gallery = RoomPresets::firstRoom();
    gallery.id = "gallery";
    game.loadRoom(gallery);
    assert(!game.inCutscene());
    assert(!requestedRoom(game).has_value());
}

RoomDialogues guardDialogues() {
    DialogueChoice ask;
    ask.id = "ask";
    ask.text = "Quem e voce?";
    ask.once = true;
    ask.effects = {InteractionEffect::make("say", {"Resposta"})};
    ask.next = "guard";
    DialogueChoice key;
    key.id = "key";
    key.text = "Tenho a chave.";
    key.conditions = {InteractionCondition::make("has_item", {"brass_key"})};
    key.effects = {InteractionEffect::make("take_item", {"brass_key"}),
                   InteractionEffect::make("set_flag", {"guard_open"})};
    DialogueChoice bye;
    bye.id = "bye";
    bye.text = "Tchau.";
    bye.effects = {InteractionEffect::make("change_room", {"gallery"})};
    return RoomDialogues{{DialogueNode{"guard", {InteractionEffect::make("say", {"Alto la!"})},
                                       {ask, key, bye}}}};
}

// A rule opens a dialogue (start_dialogue blocks under it), the menu lists only the options whose
// conditions hold, a reply with goto is read before its node reopens, once-options stay hidden
// (SessionState), and the rule resumes when the conversation ends.
void testDialogueWithChoices() {
    Game game(RoomPresets::firstRoom());
    game.setInteractions(RoomInteractions{{{
        .trigger = InteractionTrigger::interact("blue_box"),
        .conditions = {},
        .effects = {InteractionEffect::make("start_dialogue", {"guard"}),
                    InteractionEffect::make("set_flag", {"talked"})},
    }}});
    game.setDialogues(guardDialogues());
    assert(dialogueReferencesError(game.scene().interactions(), game.scene().dialogues()).empty());

    game.onClick({430.0F, 410.0F});
    for (int i = 0; i < 200 && !game.dialogueActive(); ++i) game.tick(0.05F);
    assert(game.dialogueActive() && game.inCutscene());
    // The node's line is the last step, so the menu opens with it still on screen.
    assert(game.awaitingDialogueChoice());
    assert(game.scene().floatingText().isVisible() && game.scene().floatingText().text() == "Alto la!");
    assert(game.dialogueOptions().size() == 2); // no key yet: "key" is filtered out
    assert(game.dialogueOptions()[0]->id == "ask" && game.dialogueOptions()[1]->id == "bye");

    // Modal: world clicks neither move the player nor end the conversation.
    game.onClick({200.0F, 600.0F});
    assert(!game.scene().player().moving() && game.awaitingDialogueChoice());
    assert(!game.chooseDialogueOption(5));
    assert(!game.startDialogue("guard")); // already in cutscene

    // A reply that leads to a node is read before the node reopens.
    assert(game.chooseDialogueOption(0));
    assert(!game.awaitingDialogueChoice() && game.inCutscene());
    assert(game.scene().floatingText().text() == "Resposta");
    assert(!game.chooseDialogueOption(0));
    assert(game.session().dialogueChoiceTaken("first", "guard", "ask"));
    game.tick(0.05F);
    assert(!game.awaitingDialogueChoice());
    game.onClick({200.0F, 600.0F}); // skip the reply
    game.tick(0.05F);
    assert(game.awaitingDialogueChoice());
    assert(game.scene().floatingText().text() == "Alto la!");
    assert(game.dialogueOptions().size() == 1 && game.dialogueOptions()[0]->id == "bye");

    // change_room inside a reply waits for the dialogue and the rule that opened it.
    assert(game.chooseDialogueOption(0));
    assert(!game.dialogueActive());
    assert(!requestedRoom(game).has_value());
    assert(!game.session().hasFlag("talked"));
    game.tick(0.05F);
    assert(!game.inCutscene());
    assert(game.session().hasFlag("talked"));
    assert(requestedRoom(game) && *requestedRoom(game) == "gallery");
    (void)consumeRequestedRoom(game);

    // With the key the conditional option shows; "ask" stays hidden (once, kept in session).
    game.session().addItem("brass_key");
    assert(game.startDialogue("guard"));
    assert(game.awaitingDialogueChoice());
    assert(game.dialogueOptions().size() == 2);
    assert(game.dialogueOptions()[0]->id == "key" && game.dialogueOptions()[1]->id == "bye");
    assert(game.chooseDialogueOption(0));
    assert(!game.dialogueActive() && !game.inCutscene());
    assert(game.session().hasFlag("guard_open") && !game.session().hasItem("brass_key"));
    assert(!game.startDialogue("ghost"));

    // Changing room ends the conversation and drops the old room's dialogues; progress stays.
    assert(game.startDialogue("guard"));
    assert(game.dialogueActive());
    Room gallery = RoomPresets::firstRoom();
    gallery.id = "gallery";
    game.loadRoom(gallery);
    assert(!game.dialogueActive() && !game.inCutscene() && game.scene().dialogues().empty());
    assert(game.session().dialogueChoiceTaken("first", "guard", "ask"));

    // A node with no option left ends after its lines; validation guards the definitions.
    RoomDialogues invalid = guardDialogues();
    invalid.nodes[0].choices[0].next = "nowhere";
    std::string error;
    assert(!game.setDialogues(invalid, error) && error == "Destino de goto desconhecido: nowhere");
    invalid = guardDialogues();
    invalid.nodes[0].lines.push_back(InteractionEffect::make("start_dialogue", {"guard"}));
    assert(!game.setDialogues(invalid, error)
           && error == "start_dialogue nao vale dentro de dialogos; use goto <no>");
    RoomInteractions unknown{{{.trigger = InteractionTrigger::interact("blue_box"),
                               .effects = {InteractionEffect::make("start_dialogue", {"x"})}}}};
    assert(dialogueReferencesError(unknown, guardDialogues()) == "Dialogo desconhecido: x");

    DialogueChoice locked;
    locked.id = "locked";
    locked.text = "Segredo";
    locked.conditions = {InteractionCondition::make("flag_set", {"never"})};
    game.setDialogues(RoomDialogues{{DialogueNode{
            "lonely", {InteractionEffect::make("say", {"Silencio."})}, {locked}}}});
    assert(game.startDialogue("lonely"));
    assert(!game.dialogueActive() && game.scene().floatingText().text() == "Silencio.");
}

// Game reports outward through GameEvent: loads, lines, cutscene edges and room requests, in
// the order they happened. Live edits stay silent and a room change drops stale requests.
void testGameEventQueue() {
    Room room = RoomPresets::firstRoom();
    Game game(room);
    std::vector<GameEvent> drained;
    game.drainEvents(drained);
    assert(drained.size() == 1);
    assert(std::get<RoomEntered>(drained[0]).roomId == "first");
    assert(game.pendingEvents().empty());

    std::string error;
    Room edited = room;
    edited.playerFarSpeed += 10.0F;
    assert(game.applyRoom(edited, false, error));
    assert(game.pendingEvents().empty()); // live edit: no RoomEntered

    game.setInteractions(RoomInteractions{{{
        .trigger = InteractionTrigger::interact("blue_box"),
        .conditions = {},
        .effects = {
            InteractionEffect::make("say", {"A"}),
            InteractionEffect::make("say", {"B"}),
            InteractionEffect::make("change_room", {"gallery"}),
        },
    }}});
    game.onClick({430.0F, 410.0F});
    for (int i = 0; i < 200 && !game.inCutscene(); ++i) game.tick(0.05F);
    game.onClick({200.0F, 600.0F}); // skip A
    game.tick(0.05F);               // B holds the queue: change_room still follows it
    assert(game.inCutscene() && game.scene().floatingText().text() == "B");
    assert(!requestedRoom(game));
    game.onClick({200.0F, 600.0F}); // skip B
    game.tick(0.05F);               // change_room runs, the sequence ends, the request goes out
    assert(!game.inCutscene());

    const auto events = game.pendingEvents();
    assert(events.size() == 5);
    assert(std::get<LineSpoken>(events[0]).text == "A");
    assert(std::holds_alternative<CutsceneStarted>(events[1]));
    assert(std::get<LineSpoken>(events[2]).text == "B");
    assert(std::get<RoomChangeRequested>(events[3]).roomId == "gallery");
    assert(std::holds_alternative<CutsceneEnded>(events[4]));

    // Entering another room before the shell drained the queue drops the old request.
    Room other = RoomPresets::firstRoom();
    other.id = "other";
    game.loadRoom(other);
    assert(!requestedRoom(game));
    assert(std::get<RoomEntered>(game.pendingEvents().back()).roomId == "other");

    // drainEvents hands the events over and leaves the queue empty.
    drained.reserve(32);
    game.drainEvents(drained);
    assert(drained.size() == 5);
    assert(game.pendingEvents().empty());
}

// A click on a point the player can never reach: a single flood fill picks the nearest
// reachable cell and one A* walks there, even in the largest room the format allows.
void testNearestReachableInLargeRoom() {
    Navigation navigation(Room::maxDimension, 2048.0F);
    // A closed ring: walls on columns 1 and 7 and rows 1 and 7; the player stands inside.
    for (int i = 1; i <= 7; ++i) {
        navigation.setBlocked(1, i, true);
        navigation.setBlocked(7, i, true);
        navigation.setBlocked(i, 1, true);
        navigation.setBlocked(i, 7, true);
    }
    const SDL_FPoint inside{112.0F, 112.0F}; // cell (3, 3)
    const SDL_FPoint farAway{8000.0F, 170.0F};
    const auto route = navigation.findPathToNearestReachable(inside, farAway);
    assert(!route.empty());
    // Nearest interior cell to the target: column 6, row 5 (centre 208, 176).
    assert(near(route.back().x, 208.0F) && near(route.back().y, 176.0F));
    for (const SDL_FPoint step : route) assert(navigation.canStandAt(step));

    // A reachable target still gets the exact point.
    const auto direct = navigation.findPathToNearestReachable(inside, {200.0F, 90.0F});
    assert(!direct.empty() && near(direct.back().x, 200.0F) && near(direct.back().y, 90.0F));
}

void testCameraTargetFor() {
    Camera camera(2000.0F, 1000.0F);

    // Foco no centro da viewport produz offset zero.
    SDL_FPoint target = camera.targetFor({640.0F, 360.0F});
    assert(near(target.x, 0.0F));
    assert(near(target.y, 0.0F));

    // Foco livre: o alvo centraliza o pé do jogador na viewport.
    target = camera.targetFor({1000.0F, 600.0F});
    assert(near(target.x, 360.0F));
    assert(near(target.y, 240.0F));

    // Limites: offset nunca sai de [0, room - World].
    target = camera.targetFor({1900.0F, 900.0F});
    assert(near(target.x, 720.0F));
    assert(near(target.y, 280.0F));
    target = camera.targetFor({-500.0F, -500.0F});
    assert(near(target.x, 0.0F));
    assert(near(target.y, 0.0F));

    // targetFor é const: não altera a posição atual da câmera.
    camera.snapTo({1000.0F, 600.0F});
    const SDL_FPoint before = camera.position();
    (void)camera.targetFor({100.0F, 100.0F});
    assert(near(camera.position().x, before.x));
    assert(near(camera.position().y, before.y));

    // Foco não finito devolve a posição atual, sem propagar NaN.
    const SDL_FPoint nanTarget = camera.targetFor({std::nanf(""), 10.0F});
    assert(near(nanTarget.x, camera.position().x));
    assert(near(nanTarget.y, camera.position().y));
    const SDL_FPoint infTarget = camera.targetFor({10.0F, INFINITY});
    assert(near(infTarget.x, camera.position().x));
    assert(near(infTarget.y, camera.position().y));

    // Sala menor que a viewport: a câmera fica travada em (0, 0).
    Camera small(100.0F, 100.0F);
    target = small.targetFor({1200.0F, 700.0F});
    assert(near(target.x, 0.0F));
    assert(near(target.y, 0.0F));
}

int main() {
    testMovementAndRetargeting();
    testRoomPerspective();
    testPerspectiveMovementSpeed();
    testObstacleRoute();
    testSolidObjectFootprint();
    testStandardRoomCameraFixed();
    testPanoramicRoomCameraAndNavigation();
    testWalkToInteractAndDepthPicking();
    testItemPickupAndHiddenCollision();
    testConditionalNpcPuzzleAndRoomPersistence();
    testScriptedSequenceAndCutscene();
    testDialogueWithChoices();
    testGameEventQueue();
    testNearestReachableInLargeRoom();
    testCameraTargetFor();
}
