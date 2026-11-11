#include "support/RoomPresets.hpp"
#include "content/RoomFiles.hpp"
#include "content/RoomLibrary.hpp"
#include "content/SaveFiles.hpp"
#include "content/TextFormat.hpp"
#include "game/Autosave.hpp"
#include "game/Camera.hpp"
#include "game/Game.hpp"
#include "game/Navigation.hpp"

#include "support/TempDirectory.hpp"

#include <doctest/doctest.h>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <clocale>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iterator>
#include <limits>
#include <locale>
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

TEST_CASE("testMovementAndRetargeting") {
    Game game(RoomPresets::firstRoom());
    REQUIRE(near(game.scene().player().feet().x, 640.0F));
    REQUIRE(near(game.scene().player().feet().y, 360.0F));

    game.onClick({900.0F, 360.0F});
    game.tick(0.5F);
    REQUIRE(near(game.scene().player().feet().x,
                 640.0F + game.scene().room().speedAt(360.0F) * 0.5F));
    game.onClick({750.0F, 500.0F});
    for (int i = 0; i < 20; ++i) {
        game.tick(0.05F);
    }
    REQUIRE(near(game.scene().player().feet().x, 750.0F));
    REQUIRE(near(game.scene().player().feet().y, 500.0F));
    REQUIRE(!game.scene().player().moving());

    game.onClick({-10.0F, 200.0F});
    REQUIRE(!game.scene().player().moving());
    game.onClick({2.0F, 2.0F});
    REQUIRE(near(game.scene().player().destination().x, 750.0F));
    REQUIRE(near(game.scene().player().destination().y, 500.0F));
    game.onClick({2.0F, game.scene().room().wallBottomY + 1.0F});
    const float expectedY = game.scene().room().walkableTopY;
    REQUIRE(near(game.scene().player().destination().y, expectedY));
    REQUIRE(near(game.scene().player().destination().x,
                 game.scene().room().playerBaseSize.x * game.scene().room().scaleAt(expectedY)
                         * 0.5F));
}

TEST_CASE("testRoomPerspective") {
    Room room = RoomPresets::firstRoom();
    REQUIRE(near(room.scaleAt(room.farDepthY), room.farScale));
    REQUIRE(near(room.scaleAt(room.nearDepthY), room.nearScale));
    REQUIRE(room.scaleAt(360.0F) < room.scaleAt(600.0F));
    REQUIRE(near(room.speedAt(room.farDepthY), room.playerFarSpeed));
    REQUIRE(near(room.speedAt(room.nearDepthY), room.playerNearSpeed));
    REQUIRE(room.speedAt(360.0F) < room.speedAt(600.0F));
    REQUIRE(room.playerBounds({640.0F, 360.0F}).w < room.playerBounds({640.0F, 600.0F}).w);

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
    REQUIRE(!custom.scene().player().moving());
    custom.onClick({800.0F, 400.0F});
    custom.tick(0.5F);
    REQUIRE(custom.scene().player().moving());
    REQUIRE(near(std::hypot(custom.scene().player().feet().x - room.playerStart.x,
                            custom.scene().player().feet().y - room.playerStart.y),
                 50.0F));

    Room nextRoom = RoomPresets::firstRoom();
    nextRoom.playerStart = {300.0F, 620.0F};
    custom.loadRoom(nextRoom);
    REQUIRE(near(custom.scene().player().feet().x, 300.0F));
    REQUIRE(near(custom.scene().player().feet().y, 620.0F));
    REQUIRE(!custom.scene().player().moving());
}

TEST_CASE("testPerspectiveMovementSpeed") {
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
    REQUIRE(nearDistance > farDistance);
    REQUIRE(near(farDistance, room.speedAt(320.0F) * 0.5F));
    REQUIRE(near(nearDistance, room.speedAt(650.0F) * 0.5F));
}

TEST_CASE("testObstacleRoute") {
    Navigation corner;
    corner.setBlocked(1, 1, true);
    const auto aroundCorner = corner.findPath({31, 33}, {33, 31});
    REQUIRE(aroundCorner.size() > 1);
    REQUIRE(corner.findPath({10, 10}, {10, 720}).empty());
    Navigation navigation;
    for (int row = 8; row <= 13; ++row) {
        navigation.setBlocked(19, row, true);
    }
    const SDL_FPoint start{500.0F, 360.0F};
    const SDL_FPoint goal{750.0F, 360.0F};
    const auto route = navigation.findPath(start, goal);
    REQUIRE(route.size() > 1);
    REQUIRE((near(route.back().x, goal.x) && near(route.back().y, goal.y)));

    SDL_FPoint from = start;
    for (const SDL_FPoint to : route) {
        const float length = std::hypot(to.x - from.x, to.y - from.y);
        const int steps = static_cast<int>(std::ceil(length));
        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const int column = static_cast<int>((from.x + (to.x - from.x) * t) / Navigation::cellSize);
            const int row = static_cast<int>((from.y + (to.y - from.y) * t) / Navigation::cellSize);
            REQUIRE((column != 19 || row < 8 || row > 13));
        }
        from = to;
    }
    navigation.setBlocked(23, 11, true);
    REQUIRE(navigation.findPath(start, goal).empty());

    Navigation floorOnly;
    floorOnly.setWalkableTop(300.0F);
    for (int row = 9; row <= 14; ++row) {
        floorOnly.setBlocked(19, row, true);
    }
    const auto floorRoute = floorOnly.findPath(start, goal);
    REQUIRE(!floorRoute.empty());
    for (const SDL_FPoint waypoint : floorRoute) {
        REQUIRE(waypoint.y >= 300.0F);
    }
}

TEST_CASE("testSolidObjectFootprint") {
    Room room = RoomPresets::firstRoom();
    REQUIRE(room.scenery[0].type == SceneObjectType::decoration);
    REQUIRE(room.scenery[1].type == SceneObjectType::solid);
    const SceneObject& red = room.scenery[1];
    const SDL_FRect footprint = red.worldCollisionFootprint();
    REQUIRE((footprint.w < red.bounds.w && footprint.h < red.bounds.h));

    Game game(room);
    game.onClick({860.0F, 500.0F}); // The upper visual area is passable.
    REQUIRE(near(game.scene().player().destination().x, 860.0F));
    REQUIRE(near(game.scene().player().destination().y, 500.0F));
    game.onClick({860.0F, 575.0F}); // The solid base snaps to reachable ground.
    const SDL_FPoint snapped = game.scene().player().destination();
    REQUIRE(!SDL_PointInRectFloat(&snapped, &footprint));

    room.playerStart = {730.0F, 575.0F};
    Game detour(room);
    detour.onClick({1050.0F, 575.0F});
    REQUIRE(detour.scene().player().remainingWaypoints() > 1);
    for (int i = 0; i < 100; ++i) {
        detour.tick(0.05F);
        const SDL_FPoint feet = detour.scene().player().feet();
        REQUIRE(!SDL_PointInRectFloat(&feet, &footprint));
    }
    REQUIRE(!detour.scene().player().moving());
    REQUIRE(near(detour.scene().player().feet().x, 1050.0F));
    REQUIRE(near(detour.scene().player().feet().y, 575.0F));
}

TEST_CASE("testStandardRoomCameraFixed") {
    Game game(RoomPresets::firstRoom());
    REQUIRE(game.scene().navigation().columns() == 40);
    REQUIRE(game.scene().navigation().rows() == 23);
    REQUIRE(near(game.scene().camera().position().x, 0.0F));
    REQUIRE(near(game.scene().camera().position().y, 0.0F));

    const SDL_FPoint targets[]{{80.0F, 340.0F}, {1220.0F, 680.0F}, {640.0F, 500.0F}};
    for (const SDL_FPoint target : targets) {
        game.onClick(target);
        for (int i = 0; i < 60; ++i) {
            game.tick(0.05F);
            for (const float alpha : {0.0F, 0.5F, 1.0F}) {
                const SDL_FPoint cam = game.scene().camera().interpolatedPosition(alpha);
                REQUIRE((near(cam.x, 0.0F) && near(cam.y, 0.0F)));
            }
        }
    }
}

TEST_CASE("testPanoramicRoomCameraAndNavigation") {
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
    REQUIRE(game.scene().navigation().columns() == 80);
    REQUIRE(game.scene().navigation().rows() == 23);
    // Spawn near the left border clamps the initial camera to x = 0.
    REQUIRE(near(game.scene().camera().position().x, 0.0F));
    REQUIRE(near(game.scene().camera().position().y, 0.0F));

    game.onClick({2350.0F, 560.0F});
    REQUIRE(game.scene().player().moving());
    REQUIRE(game.scene().player().remainingWaypoints() > 1);

    bool sawSmoothLag = false;
    bool crossedOriginalWidth = false;
    float previousCameraX = game.scene().camera().position().x;
    for (int i = 0; i < 220; ++i) {
        const float beforeTickCamX = game.scene().camera().position().x;
        game.tick(0.05F);
        const SDL_FPoint feet = game.scene().player().feet();
        const SDL_FPoint cam = game.scene().camera().position();
        const SDL_FPoint mid = game.scene().camera().interpolatedPosition(0.5F);
        REQUIRE(!SDL_PointInRectFloat(&feet, &eastFootprint));
        REQUIRE((cam.x >= 0.0F && cam.x <= room.width - World::width));
        REQUIRE(near(cam.y, 0.0F));
        REQUIRE(cam.x + 0.001F >= previousCameraX);
        REQUIRE((mid.x >= beforeTickCamX - 0.001F && mid.x <= cam.x + 0.001F));
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
    REQUIRE(crossedOriginalWidth);
    REQUIRE(sawSmoothLag);
    REQUIRE(!game.scene().player().moving());
    REQUIRE(near(game.scene().player().feet().x, 2350.0F));
    // Near the right border (2350 - 640 = 1710 > 1280), the camera clamps at 1280.
    REQUIRE(near(game.scene().camera().position().x, 1280.0F));

    // Clicking inside the solid footprint at x > 1280 snaps to nearest walkable ground.
    game.onClick({1800.0F, 565.0F});
    const SDL_FPoint snapped = game.scene().player().destination();
    REQUIRE(!SDL_PointInRectFloat(&snapped, &eastFootprint));

    // Teleporting snaps the camera immediately with zero lag.
    std::string error;
    REQUIRE(game.teleportPlayer({1400.0F, 520.0F}, error));
    REQUIRE(near(game.scene().camera().position().x, 760.0F));
    REQUIRE(near(game.scene().camera().interpolatedPosition(0.0F).x, 760.0F));

    // Live editor tweaks (resetPlayer == false) keep continuity and clamp when shrinking.
    Room faster = game.scene().room();
    faster.playerFarSpeed = 160.0F;
    REQUIRE(game.applyRoom(faster, false, error));
    REQUIRE(near(game.scene().camera().position().x, 760.0F));

    Room narrower = game.scene().room();
    narrower.scenery.clear();
    narrower.width = 1800.0F;
    REQUIRE(game.applyRoom(narrower, false, error));
    REQUIRE(near(game.scene().camera().position().x, 520.0F));

    // Loading a room strong-snaps both X and Y camera axes to the spawn target.
    Room tallAndWide = RoomPresets::firstRoom();
    tallAndWide.scenery.clear();
    tallAndWide.width = 2560.0F;
    tallAndWide.height = 1440.0F;
    tallAndWide.nearDepthY = 1440.0F;
    tallAndWide.playerStart = {1900.0F, 1000.0F};
    game.loadRoom(tallAndWide);
    REQUIRE(near(game.scene().camera().position().x, 1260.0F));
    REQUIRE(near(game.scene().camera().position().y, 640.0F));
}

TEST_CASE("testWalkToInteractAndDepthPicking") {
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
    // start walking, and not trigger the effect before arrival.
    game.onClick({740.0F, 470.0F});
    REQUIRE(game.scene().player().moving());
    REQUIRE(game.scene().pendingInteraction().has_value());
    REQUIRE(game.scene().pendingInteraction()->objectId == "front_banner");
    REQUIRE(!game.session().hasFlag("took_banner"));
    REQUIRE(!game.scene().floatingText().isVisible());

    game.tick(0.25F);
    REQUIRE(game.scene().player().moving());
    REQUIRE(!game.session().hasFlag("took_banner"));

    // 2. Clicking empty floor mid-walk cancels the pending interaction and reroutes.
    game.onClick({320.0F, 600.0F});
    REQUIRE(!game.scene().pendingInteraction().has_value());
    for (int i = 0; i < 80; ++i) {
        game.tick(0.05F);
    }
    REQUIRE(!game.scene().player().moving());
    REQUIRE((near(game.scene().player().feet().x, 320.0F)
             && near(game.scene().player().feet().y, 600.0F)));
    REQUIRE(!game.session().hasFlag("took_banner"));
    REQUIRE(game.scene().isObjectVisible("front_banner"));

    // 3. Click the overlapping spot again and walk all the way to arrival: front_banner fires and hides.
    game.onClick({740.0F, 470.0F});
    REQUIRE(game.scene().pendingInteraction().has_value());
    REQUIRE(game.scene().pendingInteraction()->objectId == "front_banner");
    for (int i = 0; i < 100; ++i) {
        game.tick(0.05F);
    }
    REQUIRE(!game.scene().player().moving());
    REQUIRE(!game.scene().pendingInteraction().has_value());
    REQUIRE(game.session().hasFlag("took_banner"));
    REQUIRE(!game.scene().isObjectVisible("front_banner"));
    REQUIRE(game.scene().floatingText().text() == "Retirei o estandarte da frente.");
    REQUIRE(!game.session().hasFlag("inspected_statue"));

    // 4. Clicking the exact same spot (740, 470) now ignores hidden front_banner and picks back_statue.
    game.onClick({740.0F, 470.0F});
    for (int i = 0; i < 60; ++i) {
        game.tick(0.05F);
    }
    REQUIRE(!game.scene().player().moving());
    REQUIRE(game.session().hasFlag("inspected_statue"));
    REQUIRE(game.scene().floatingText().text() == "Uma estatua antiga.");

    // 5. Clicking back_statue while ALREADY standing at its interaction point fires immediately.
    game.changeSession([](SessionState& session) { session.clearFlag("inspected_statue"); });
    game.onClick({740.0F, 470.0F});
    REQUIRE(!game.scene().player().moving());
    REQUIRE(!game.scene().pendingInteraction().has_value());
    REQUIRE(game.session().hasFlag("inspected_statue"));

    // 6. Clicking wall_painting above wallBottomY walks to walkableTopY beneath it and interacts.
    game.onClick({465.0F, 175.0F});
    REQUIRE(game.scene().player().moving());
    for (int i = 0; i < 80; ++i) {
        game.tick(0.05F);
    }
    REQUIRE(!game.scene().player().moving());
    REQUIRE(near(game.scene().player().feet().y, room.walkableTopY));
    REQUIRE(game.session().hasFlag("saw_painting"));
    REQUIRE(game.scene().floatingText().text() == "Um retrato na parede.");

    // 7. A few pixels off that point but within reach: the interaction fires where the player
    // stands, with no shuffle to the centre of the navigation cell first.
    const SDL_FPoint arrived = game.scene().player().feet();
    std::string nudgeError;
    REQUIRE(game.teleportPlayer({arrived.x + 9.0F, arrived.y + 5.0F}, nudgeError));
    const SDL_FPoint nudged = game.scene().player().feet();
    REQUIRE(!near(nudged.x, arrived.x));
    game.changeSession([](SessionState& session) { session.clearFlag("saw_painting"); });
    game.onClick({465.0F, 175.0F});
    REQUIRE(!game.scene().player().moving());
    REQUIRE(!game.scene().pendingInteraction().has_value());
    REQUIRE(game.session().hasFlag("saw_painting"));
    REQUIRE(near(game.scene().player().feet().x, nudged.x));
    REQUIRE(near(game.scene().player().feet().y, nudged.y));

    // 8. Farther than one navigation cell, the player still walks up to the object first.
    REQUIRE(game.teleportPlayer({arrived.x + 60.0F, arrived.y + 40.0F}, nudgeError));
    game.changeSession([](SessionState& session) { session.clearFlag("saw_painting"); });
    game.onClick({465.0F, 175.0F});
    REQUIRE(game.scene().player().moving());
    REQUIRE(game.scene().pendingInteraction().has_value());
    REQUIRE(!game.session().hasFlag("saw_painting"));
}

TEST_CASE("testItemPickupAndHiddenCollision") {
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
    REQUIRE(!game.scene().navigation().canStandAt(crateCenter));

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
    REQUIRE(game.scene().player().moving());
    REQUIRE(!game.session().hasItem("crowbar"));
    for (int i = 0; i < 80; ++i) {
        game.tick(0.05F);
    }
    REQUIRE(!game.scene().player().moving());
    REQUIRE(game.session().hasItem("crowbar"));
    REQUIRE(game.session().items().size() == 1);
    REQUIRE(game.session().items()[0] == "crowbar");
    REQUIRE(!game.scene().isObjectVisible("supply_crate"));
    REQUIRE(!game.scene().isObjectSolid(game.scene().room().scenery[0]));
    // Hidden solid object no longer blocks navigation, and clicking its old bounds walks onto the floor.
    REQUIRE(game.scene().navigation().canStandAt(crateCenter));

    game.onClick(crateCenter);
    REQUIRE(!game.scene().pendingInteraction().has_value());
    for (int i = 0; i < 40; ++i) {
        game.tick(0.05F);
    }
    REQUIRE(near(game.scene().player().feet().x, crateCenter.x));
    REQUIRE(near(game.scene().player().feet().y, crateCenter.y));
    REQUIRE(!game.scene().floatingText().isVisible());
}

// Progress changed from outside a script goes through Game::changeSession, which rebuilds
// navigation when an object of the room starts or stops blocking the floor.
TEST_CASE("testSessionCommandKeepsNavigation") {
    Room room = RoomPresets::firstRoom();
    room.playerStart = {250.0F, 520.0F};
    room.scenery = {
        {{580.0F, 470.0F, 90.0F, 90.0F}, {210, 175, 65, 255}, SceneObjectType::solid,
         {10.0F, 60.0F, 70.0F, 30.0F}, "supply_crate", {}, "idle"},
    };
    const SDL_FPoint crateCenter{625.0F, 545.0F};
    const SDL_FRect footprint = room.scenery[0].worldCollisionFootprint();

    Game game(room);
    const std::string roomId = game.scene().room().id;
    const SceneObject& crate = game.scene().room().scenery[0];
    std::string error;
    REQUIRE(!game.scene().navigation().canStandAt(crateCenter));
    REQUIRE(!game.teleportPlayer(crateCenter, error));

    // A change that leaves collision alone keeps navigation and the walk in progress.
    game.onClick({400.0F, 600.0F});
    REQUIRE(game.scene().player().moving());
    game.changeSession([](SessionState& session) {
        session.setFlag("lever_pulled");
        session.addItem("crowbar");
    });
    REQUIRE((game.session().hasFlag("lever_pulled") && game.session().hasItem("crowbar")));
    REQUIRE(game.scene().player().moving());
    REQUIRE(!game.scene().navigation().canStandAt(crateCenter));

    // Collision turned off: the cells are free at once, not only isObjectSolid.
    game.changeSession([&](SessionState& session) {
        session.setObjectSolid(roomId, "supply_crate", false);
    });
    REQUIRE(!game.scene().isObjectSolid(crate));
    REQUIRE(game.scene().navigation().canStandAt(crateCenter));
    REQUIRE(game.teleportPlayer(crateCenter, error));

    // Solid again under the player's feet: the cells block and the player is moved out.
    game.changeSession([&](SessionState& session) {
        session.setObjectSolid(roomId, "supply_crate", true);
    });
    REQUIRE(!game.scene().navigation().canStandAt(crateCenter));
    const SDL_FPoint pushed = game.scene().player().feet();
    REQUIRE(!SDL_PointInRectFloat(&pushed, &footprint));
    REQUIRE(game.scene().navigation().canStandAt(pushed));

    // Hiding frees the floor too; wiping the progress brings the authored collision back.
    game.changeSession([&](SessionState& session) {
        session.setObjectVisible(roomId, "supply_crate", false);
    });
    REQUIRE(game.scene().navigation().canStandAt(crateCenter));
    game.changeSession([](SessionState& session) { session.reset(); });
    REQUIRE(game.scene().isObjectSolid(crate));
    REQUIRE(!game.scene().navigation().canStandAt(crateCenter));

    // An object of another room with the same id is not this room's collision.
    game.changeSession([](SessionState& session) {
        session.setObjectSolid("another_room", "supply_crate", false);
    });
    REQUIRE(!game.scene().navigation().canStandAt(crateCenter));
}

TEST_CASE("testConditionalNpcPuzzleAndRoomPersistence") {
    Room room = RoomPresets::firstRoom();
    room.id = "courtyard";
    room.playerStart = {200.0F, 520.0F};
    SpriteClip closedClip{.file = "door_closed.png"};
    SpriteClip openClip{.file = "door_open.png"};
    AnimationSet doorAnims{{"idle", closedClip}, {"open", openClip}};

    // Gate spans the entire walkable height at x = 700, partitioning the room until unlocked.
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
    // and must not collect relic through the solid wall.
    game.onClick({1055.0F, 515.0F});
    for (int i = 0; i < 100; ++i) {
        game.tick(0.05F);
    }
    REQUIRE(game.scene().player().feet().x < 700.0F);
    REQUIRE(!game.session().hasItem("relic"));
    REQUIRE(game.scene().isObjectVisible("relic"));
    REQUIRE(!requestedRoom(game).has_value());

    // 2. Talk to guard before having silver_key: sets talked_to_guard flag, gate stays solid.
    game.onClick({535.0F, 430.0F});
    for (int i = 0; i < 80; ++i) {
        game.tick(0.05F);
    }
    REQUIRE(game.session().hasFlag("talked_to_guard"));
    REQUIRE(!game.session().hasFlag("gate_open"));
    REQUIRE(game.scene().floatingText().text() == "Sem a chave prateada ninguem passa.");
    REQUIRE(!game.scene().navigation().canStandAt({730.0F, 520.0F}));
    REQUIRE(game.scene().objectAnimation("iron_gate")->name() == "idle");

    // 3. Pick up silver_key on the floor.
    game.onClick({390.0F, 495.0F});
    for (int i = 0; i < 80; ++i) {
        game.tick(0.05F);
    }
    REQUIRE(game.session().hasItem("silver_key"));
    REQUIRE(!game.scene().isObjectVisible("silver_key"));

    // 4. Talk to guard with silver_key and talked_to_guard: key consumed, gate_open set,
    // iron_gate plays "open" and becomes passable.
    game.onClick({535.0F, 430.0F});
    for (int i = 0; i < 80; ++i) {
        game.tick(0.05F);
    }
    REQUIRE(!game.session().hasItem("silver_key"));
    REQUIRE(game.session().hasFlag("gate_open"));
    REQUIRE(game.scene().floatingText().text() == "Essa e a chave! Vou abrir o portao.");
    REQUIRE(game.scene().objectAnimation("iron_gate")->name() == "open");
    REQUIRE(game.scene().navigation().canStandAt({730.0F, 520.0F}));

    // Talking to guard a third time (already standing next to him) reflects the new state.
    game.onClick({535.0F, 430.0F});
    REQUIRE(game.scene().floatingText().text() == "O portao esta aberto. Boa sorte la dentro.");

    // 5. Walk through the unlocked gate to collect relic and trigger change_room "sanctuary".
    game.onClick({1055.0F, 515.0F});
    for (int i = 0; i < 120; ++i) {
        game.tick(0.05F);
    }
    REQUIRE(game.scene().player().feet().x > 950.0F);
    REQUIRE(game.session().hasItem("relic"));
    REQUIRE(!game.scene().isObjectVisible("relic"));
    REQUIRE(requestedRoom(game).has_value());
    REQUIRE(*requestedRoom(game) == "sanctuary");
    REQUIRE(*consumeRequestedRoom(game) == "sanctuary");
    REQUIRE(!requestedRoom(game).has_value());

    // 6. Transitioning to another room and returning preserves SessionState (inventory, flags,
    // hidden items, and unlocked iron_gate).
    Room sanctuary = RoomPresets::firstRoom();
    sanctuary.id = "sanctuary";
    game.loadRoom(sanctuary);
    REQUIRE(game.session().hasItem("relic"));
    REQUIRE(game.session().hasFlag("gate_open"));

    game.loadRoom(room);
    REQUIRE(!game.scene().isObjectVisible("silver_key"));
    REQUIRE(!game.scene().isObjectVisible("relic"));
    REQUIRE(game.scene().navigation().canStandAt({730.0F, 520.0F}));
}
}

// walk_to -> say -> play_animation -> wait_animation -> wait -> say -> change_room runs over
// several ticks; while it holds the scene, clicks only skip the current line.
TEST_CASE("testScriptedSequenceAndCutscene") {
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
    REQUIRE(game.inCutscene());
    REQUIRE(game.scene().player().moving()); // walk_to red_box holds the queue

    // World clicks do not redirect the scripted walk.
    const SDL_FPoint destination = game.scene().player().destination();
    game.onClick({200.0F, 600.0F});
    REQUIRE(game.scene().player().destination().x == destination.x);
    REQUIRE(game.scene().player().destination().y == destination.y);
    REQUIRE(!game.useItemOnObject("anything", "red_box"));

    for (int i = 0; i < 300 && game.scene().player().moving(); ++i) game.tick(0.05F);
    REQUIRE((game.scene().floatingText().isVisible() && game.scene().floatingText().text() == "A"));
    REQUIRE(game.inCutscene());
    REQUIRE(game.scene().objectAnimation("red_box")->name() == "idle");

    // A click skips the line; the next tick starts the animation and waits for it.
    game.onClick({200.0F, 600.0F});
    REQUIRE(!game.scene().floatingText().isVisible());
    REQUIRE(!game.scene().player().moving());
    game.tick(0.05F);
    REQUIRE(game.scene().objectAnimation("red_box")->name() == "open");
    REQUIRE(game.inCutscene());

    for (int i = 0; i < 12; ++i) game.tick(0.05F); // clip (0.5 s) ends, wait 0.5 starts
    REQUIRE(game.inCutscene());
    REQUIRE(game.scene().floatingText().text() == "A");
    for (int i = 0; i < 12; ++i) game.tick(0.05F); // wait ends, say B holds the queue
    REQUIRE(game.scene().floatingText().text() == "B");
    REQUIRE(game.inCutscene());
    REQUIRE(!game.session().hasFlag("cutscene_done"));
    REQUIRE(!requestedRoom(game).has_value());

    // B expires on its own (reading time); the rest runs and the room change is published.
    for (int i = 0; i < 400 && game.inCutscene(); ++i) game.tick(0.05F);
    REQUIRE(!game.inCutscene());
    REQUIRE(game.session().hasFlag("cutscene_done"));
    REQUIRE((requestedRoom(game) && *requestedRoom(game) == "gallery"));

    // Changing room cancels any running sequence.
    game.onClick({430.0F, 410.0F});
    for (int i = 0; i < 200 && !game.inCutscene(); ++i) game.tick(0.05F);
    REQUIRE(game.inCutscene());
    Room gallery = RoomPresets::firstRoom();
    gallery.id = "gallery";
    game.loadRoom(gallery);
    REQUIRE(!game.inCutscene());
    REQUIRE(!requestedRoom(game).has_value());

    // A still image (fps 0, no loop) has nothing left to play: wait_animation never holds the
    // cutscene on it.
    Room still = RoomPresets::firstRoom();
    still.scenery[1].animations["idle"] = SpriteClip{.file = "red_idle.png"};
    still.scenery[1].animations["opened"] = SpriteClip{.file = "red_opened.png", .fps = 0.0F,
                                                       .loop = false};
    Game stillGame(still);
    stillGame.setInteractions(RoomInteractions{{{
        .trigger = InteractionTrigger::interact("blue_box"),
        .conditions = {},
        .effects = {
            InteractionEffect::make("play_animation", {"red_box", "opened"}),
            InteractionEffect::make("wait_animation", {"red_box"}),
            InteractionEffect::make("set_flag", {"still_done"}),
        },
    }}});
    stillGame.onClick({430.0F, 410.0F});
    for (int i = 0; i < 200 && !stillGame.session().hasFlag("still_done"); ++i) {
        stillGame.tick(0.05F);
    }
    REQUIRE(stillGame.session().hasFlag("still_done"));
    REQUIRE(!stillGame.inCutscene());
    REQUIRE(stillGame.scene().objectAnimation("red_box")->name() == "opened");

    // A line still on screen (a trailing say is not waited for) stays in the room it was said in.
    Game talker(RoomPresets::firstRoom());
    talker.setInteractions(RoomInteractions{{{
        .trigger = InteractionTrigger::interact("blue_box"),
        .conditions = {},
        .effects = {
            InteractionEffect::make("change_room", {"gallery"}),
            InteractionEffect::make("say", {"Tchau"}),
        },
    }}});
    talker.onClick({430.0F, 410.0F});
    for (int i = 0; i < 200 && !requestedRoom(talker); ++i) talker.tick(0.05F);
    REQUIRE((requestedRoom(talker) && *requestedRoom(talker) == "gallery"));
    REQUIRE(talker.scene().floatingText().isVisible());
    talker.loadRoom(gallery);
    REQUIRE(!talker.scene().floatingText().isVisible());
    // A live edit of the same room (resetPlayer = false) keeps the line being read.
    Game editing(RoomPresets::firstRoom());
    editing.setInteractions(RoomPresets::firstRoomInteractions());
    editing.onClick({430.0F, 410.0F});
    for (int i = 0; i < 200 && !editing.scene().floatingText().isVisible(); ++i) editing.tick(0.05F);
    REQUIRE(editing.scene().floatingText().isVisible());
    std::string editError;
    REQUIRE(editing.applyRoom(RoomPresets::firstRoom(), false, editError));
    REQUIRE(editing.scene().floatingText().isVisible());
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

// A rule opens a dialogue: only options whose conditions hold are listed, a goto reply is read
// before its node reopens, once-options stay hidden and the rule resumes at the end.
// F5: add_flag counts, flag_at_least / flag_equals pick the rule by the counter's value.
TEST_CASE("testFlagCounters") {
    Game game(RoomPresets::firstRoom());
    const auto say = [](std::string text) { return InteractionEffect::make("say", {std::move(text)}); };
    game.setInteractions(RoomInteractions{{
        {.trigger = InteractionTrigger::interact("blue_box"),
         .conditions = {InteractionCondition::make("flag_equals", {"knocks", "2"})},
         .effects = {say("Dois"), InteractionEffect::make("add_flag", {"knocks", "-2"})}},
        {.trigger = InteractionTrigger::interact("blue_box"),
         .conditions = {InteractionCondition::make("flag_at_least", {"knocks", "3"})},
         .effects = {say("Muitos")}},
        {.trigger = InteractionTrigger::interact("blue_box"),
         .conditions = {},
         .effects = {say("Toc"), InteractionEffect::make("add_flag", {"knocks", "1"})}},
    }});
    const auto knock = [&game]() {
        game.onClick({430.0F, 410.0F});
        for (int i = 0; i < 400 && !game.scene().floatingText().isVisible(); ++i) game.tick(0.05F);
        const std::string line = game.scene().floatingText().text();
        for (int i = 0; i < 400 && game.inCutscene(); ++i) game.tick(0.05F);
        REQUIRE(!game.inCutscene());
        return line;
    };
    REQUIRE((knock() == "Toc" && game.session().flagValue("knocks") == 1));
    REQUIRE((knock() == "Toc" && game.session().flagValue("knocks") == 2));
    REQUIRE((knock() == "Dois" && game.session().flagValue("knocks") == 0));
    REQUIRE(!game.session().hasFlag("knocks")); // 0 clears the flag
    game.changeSession([](SessionState& session) { session.setFlag("knocks", 5); });
    REQUIRE((knock() == "Muitos" && game.session().flagValue("knocks") == 5));
}

// C13: a counter saturates at +-kMaxFlagValue instead of overflowing int, and a snapshot or save
// outside the limit is refused (INT_MAX in save.slot followed by `add_flag x 1` was signed
// overflow).
TEST_CASE("testFlagCounterLimits") {
    Game game(RoomPresets::firstRoom());
    game.setInteractions(RoomInteractions{{
        {.trigger = InteractionTrigger::interact("blue_box"),
         .conditions = {},
         .effects = {InteractionEffect::make("add_flag", {"up", "1000000"}),
                     InteractionEffect::make("add_flag", {"down", "-1000000"}),
                     InteractionEffect::make("add_flag", {"mid", "-3"}),
                     InteractionEffect::make("set_flag", {"done"})}},
    }});
    const auto use = [&game]() {
        game.changeSession([](SessionState& session) { session.clearFlag("done"); });
        game.onClick({430.0F, 410.0F});
        for (int i = 0; i < 400 && !game.session().hasFlag("done"); ++i) game.tick(0.05F);
        REQUIRE(game.session().hasFlag("done"));
    };
    game.changeSession([](SessionState& session) {
        session.setFlag("up", kMaxFlagValue - 1);
        session.setFlag("down", -kMaxFlagValue + 1);
        session.setFlag("mid", 2);
    });
    use();
    REQUIRE(game.session().flagValue("up") == kMaxFlagValue);
    REQUIRE(game.session().flagValue("down") == -kMaxFlagValue);
    REQUIRE(game.session().flagValue("mid") == -1); // inside the limit nothing changes
    use(); // already at the limit: stays there
    REQUIRE(game.session().flagValue("up") == kMaxFlagValue);
    REQUIRE(game.session().flagValue("down") == -kMaxFlagValue);
    REQUIRE(game.session().flagValue("mid") == -4);
    REQUIRE(game.session().snapshot().validationError().empty());

    // The same limit outside a script, including the extremes of int.
    SessionState session;
    session.setFlag("a", std::numeric_limits<int>::max());
    session.setFlag("b", std::numeric_limits<int>::min());
    REQUIRE((session.flagValue("a") == kMaxFlagValue && session.flagValue("b") == -kMaxFlagValue));
    REQUIRE(session.incrementFlag("a", std::numeric_limits<int>::max()) == kMaxFlagValue);
    REQUIRE(session.incrementFlag("b", std::numeric_limits<int>::min()) == -kMaxFlagValue);
    REQUIRE(session.incrementFlag("a", std::numeric_limits<int>::min()) == -kMaxFlagValue);
    REQUIRE(session.incrementFlag("c", std::numeric_limits<int>::max()) == kMaxFlagValue);
    REQUIRE((session.incrementFlag("c", -kMaxFlagValue) == 0 && !session.hasFlag("c")));

    // A snapshot outside the limit is refused whole; the limit itself is a valid value.
    SessionSnapshot snapshot;
    snapshot.flags = {{"x", kMaxFlagValue + 1}};
    std::string error;
    REQUIRE(!session.restore(snapshot, error));
    REQUIRE(error == "Flag fora do limite de +-1000000000: x = 1000000001");
    REQUIRE(session.flagValue("a") == -kMaxFlagValue); // nothing changed
    snapshot.flags = {{"x", -kMaxFlagValue}, {"y", kMaxFlagValue}};
    REQUIRE((session.restore(snapshot, error) && session.flagValue("x") == -kMaxFlagValue));

    // save.slot: the limit round-trips, one past it (or the old INT_MAX) is refused by name.
    const std::string header = "version = 2\nroom = first\nfeet = 10, 20\n";
    const SaveLoadResult edge =
            SaveFiles::parse(header + "flag.x = 1000000000\nflag.y = -1000000000\n");
    REQUIRE((edge.save && edge.save->session.flags.size() == 2));
    REQUIRE(SaveFiles::parse(SaveFiles::serialize(*edge.save)).save == edge.save);
    const SaveLoadResult intMax = SaveFiles::parse(header + "flag.x = 2147483647\n");
    REQUIRE(!intMax.save);
    REQUIRE(intMax.error == "Flag fora do limite de +-1000000000: x = 2147483647");
    const SaveLoadResult below = SaveFiles::parse(header + "flag.ok = 7\nflag.x = -1000000001\n");
    REQUIRE(!below.save);
    REQUIRE(below.error == "Flag fora do limite de +-1000000000: x = -1000000001");
    // Past int itself the number does not even parse.
    REQUIRE(SaveFiles::parse(header + "flag.x = 99999999999\n").error
            == "Linha 4: Chave ou valor invalido: flag.x");
}

// F6: `face` turns the standing player; idle_<facing> is the clip drawn afterwards.
TEST_CASE("testFaceDirection") {
    Room room = RoomPresets::firstRoom();
    room.playerAnimations["idle_up"] = room.playerAnimations.at("idle");
    room.playerAnimations["idle_left"] = room.playerAnimations.at("idle");
    Game game(room);
    game.setInteractions(RoomInteractions{{
        {.trigger = InteractionTrigger::interact("blue_box"),
         .conditions = {InteractionCondition::make("flag_set", {"turned"})},
         .effects = {InteractionEffect::make("face", {"left"})}},
        {.trigger = InteractionTrigger::interact("blue_box"),
         .conditions = {},
         .effects = {InteractionEffect::make("face", {"up"}),
                     InteractionEffect::make("set_flag", {"turned"})}},
    }});
    const auto use = [&game]() {
        game.onClick({430.0F, 410.0F});
        for (int i = 0; i < 400 && !game.session().hasFlag("turned"); ++i) game.tick(0.05F);
        for (int i = 0; i < 10; ++i) game.tick(0.05F);
        REQUIRE((!game.inCutscene() && !game.scene().player().moving()));
    };
    use();
    REQUIRE(game.scene().player().facing() == Facing::up);
    REQUIRE(playerClipName(game.scene().player(), game.scene().room().playerAnimations)
            == "idle_up");
    use();
    REQUIRE(game.scene().player().facing() == Facing::left);
    REQUIRE(playerClipName(game.scene().player(), game.scene().room().playerAnimations)
            == "idle_left");
}

// F4 / C12: wildcard rules answer item uses and clicks that have no specific rule. Levels
// (most specific first): exact, `* obj`, `item *`, `* *`; file order only breaks ties.
TEST_CASE("testWildcardRules") {
    const auto say = [](std::string text) { return InteractionEffect::make("say", {std::move(text)}); };
    const auto rule = [&say](InteractionTrigger trigger, std::string text,
                             std::vector<InteractionCondition> conditions = {}) {
        return InteractionRule{.trigger = std::move(trigger), .conditions = std::move(conditions),
                               .effects = {say(std::move(text))}};
    };
    const auto spoken = [](Game& game, const std::function<bool()>& action) {
        // The last `say` of a rule is not awaited: let the previous line expire first.
        for (int i = 0; i < 600 && game.scene().floatingText().isVisible(); ++i) game.tick(0.05F);
        if (!action()) return std::string("<recusado>");
        std::string line;
        for (int i = 0; i < 600; ++i) {
            if (game.scene().floatingText().isVisible()) line = game.scene().floatingText().text();
            if (!line.empty() && !game.inCutscene()) break;
            game.tick(0.05F);
        }
        for (int i = 0; i < 400 && (game.inCutscene() || game.scene().player().moving()); ++i) {
            game.tick(0.05F);
        }
        return line.empty() ? std::string("<nada>") : line;
    };

    // Without wildcards nothing changes: an item with no rule gets no answer (C12 as before).
    {
        Game game(RoomPresets::firstRoom());
        game.setInteractions(RoomInteractions{{rule(InteractionTrigger::useItem("key", "red_box"),
                                                    "Abriu")}});
        game.changeSession([](SessionState& session) { session.addItem("pebble"); });
        REQUIRE(spoken(game, [&] { return game.useItemOnObject("pebble", "red_box"); })
                == "<nada>");
    }

    Game game(RoomPresets::firstRoom());
    // Wildcards listed first on purpose: specificity, not file order, decides.
    game.setInteractions(RoomInteractions{{
        rule(InteractionTrigger::useItem("*", "*"), "Isso nao funciona."),
        rule(InteractionTrigger::useItem("pebble", "*"), "Pedra nao serve."),
        rule(InteractionTrigger::useItem("*", "red_box"), "A caixa recusa."),
        rule(InteractionTrigger::interact("*"), "Nada de interessante."),
        rule(InteractionTrigger::useItem("key", "red_box"), "Abriu"),
        rule(InteractionTrigger::interact("blue_box"), "Azul",
             {InteractionCondition::make("flag_set", {"lit"})}),
    }});
    game.changeSession([](SessionState& session) {
        session.addItem("pebble");
        session.addItem("key");
        session.addItem("rope");
    });
    const auto use = [&](const char* item, const char* object) {
        return spoken(game, [&] { return game.useItemOnObject(item, object); });
    };
    REQUIRE(use("key", "red_box") == "Abriu");                // exact
    REQUIRE(use("pebble", "red_box") == "A caixa recusa.");   // `* obj` beats `item *`
    REQUIRE(use("pebble", "blue_box") == "Pedra nao serve."); // `item *`
    REQUIRE(use("rope", "blue_box") == "Isso nao funciona."); // `* *`
    REQUIRE(use("key", "blue_box") == "Isso nao funciona.");
    REQUIRE(use("ghost_item", "blue_box") == "<recusado>"); // not held: no walk, no rule

    // `[interact *]` answers the click; a specific rule whose condition fails falls through.
    const auto click = [&] { return spoken(game, [&] { game.onClick({430.0F, 410.0F}); return true; }); };
    REQUIRE(click() == "Nada de interessante.");
    game.changeSession([](SessionState& session) { session.setFlag("lit"); });
    REQUIRE(click() == "Azul");
}

TEST_CASE("testDialogueWithChoices") {
    Game game(RoomPresets::firstRoom());
    game.setInteractions(RoomInteractions{{{
        .trigger = InteractionTrigger::interact("blue_box"),
        .conditions = {},
        .effects = {InteractionEffect::make("start_dialogue", {"guard"}),
                    InteractionEffect::make("set_flag", {"talked"})},
    }}});
    game.setDialogues(guardDialogues());
    REQUIRE(dialogueReferencesError(game.scene().interactions(), game.scene().dialogues()).empty());

    game.onClick({430.0F, 410.0F});
    for (int i = 0; i < 200 && !game.dialogueActive(); ++i) game.tick(0.05F);
    REQUIRE((game.dialogueActive() && game.inCutscene()));
    // The node's line is the last step, so the menu opens with it still on screen.
    REQUIRE(game.awaitingDialogueChoice());
    REQUIRE((game.scene().floatingText().isVisible()
             && game.scene().floatingText().text() == "Alto la!"));
    REQUIRE(game.dialogueOptions().size() == 2); // no key yet: "key" is filtered out
    REQUIRE((game.dialogueOptions()[0]->id == "ask" && game.dialogueOptions()[1]->id == "bye"));

    // Modal: world clicks neither move the player nor end the conversation.
    game.onClick({200.0F, 600.0F});
    REQUIRE((!game.scene().player().moving() && game.awaitingDialogueChoice()));
    REQUIRE(!game.chooseDialogueOption(5));
    REQUIRE(!game.startDialogue("guard")); // already in cutscene

    // A reply that leads to a node is read before the node reopens.
    REQUIRE(game.chooseDialogueOption(0));
    REQUIRE((!game.awaitingDialogueChoice() && game.inCutscene()));
    REQUIRE(game.scene().floatingText().text() == "Resposta");
    REQUIRE(!game.chooseDialogueOption(0));
    REQUIRE(game.session().dialogueChoiceTaken("first", "guard", "ask"));
    game.tick(0.05F);
    REQUIRE(!game.awaitingDialogueChoice());
    game.onClick({200.0F, 600.0F}); // skip the reply
    game.tick(0.05F);
    REQUIRE(game.awaitingDialogueChoice());
    REQUIRE(game.scene().floatingText().text() == "Alto la!");
    REQUIRE((game.dialogueOptions().size() == 1 && game.dialogueOptions()[0]->id == "bye"));

    // change_room inside a reply waits for the dialogue and the rule that opened it.
    REQUIRE(game.chooseDialogueOption(0));
    REQUIRE(!game.dialogueActive());
    REQUIRE(!requestedRoom(game).has_value());
    REQUIRE(!game.session().hasFlag("talked"));
    game.tick(0.05F);
    REQUIRE(!game.inCutscene());
    REQUIRE(game.session().hasFlag("talked"));
    REQUIRE((requestedRoom(game) && *requestedRoom(game) == "gallery"));
    (void)consumeRequestedRoom(game);

    // With the key the conditional option shows; "ask" stays hidden (once, kept in session).
    game.changeSession([](SessionState& session) { session.addItem("brass_key"); });
    REQUIRE(game.startDialogue("guard"));
    REQUIRE(game.awaitingDialogueChoice());
    REQUIRE(game.dialogueOptions().size() == 2);
    REQUIRE((game.dialogueOptions()[0]->id == "key" && game.dialogueOptions()[1]->id == "bye"));
    REQUIRE(game.chooseDialogueOption(0));
    REQUIRE((!game.dialogueActive() && !game.inCutscene()));
    REQUIRE((game.session().hasFlag("guard_open") && !game.session().hasItem("brass_key")));
    REQUIRE(!game.startDialogue("ghost"));

    // Changing room ends the conversation and drops the old room's dialogues; progress stays.
    REQUIRE(game.startDialogue("guard"));
    REQUIRE(game.dialogueActive());
    Room gallery = RoomPresets::firstRoom();
    gallery.id = "gallery";
    game.loadRoom(gallery);
    REQUIRE((!game.dialogueActive() && !game.inCutscene() && game.scene().dialogues().empty()));
    REQUIRE(game.session().dialogueChoiceTaken("first", "guard", "ask"));

    // A node with no option left ends after its lines; validation guards the definitions.
    RoomDialogues invalid = guardDialogues();
    invalid.nodes[0].choices[0].next = "nowhere";
    std::string error;
    REQUIRE((!game.setDialogues(invalid, error)
             && error == "Destino de goto desconhecido: nowhere"));
    invalid = guardDialogues();
    invalid.nodes[0].lines.push_back(InteractionEffect::make("start_dialogue", {"guard"}));
    REQUIRE((!game.setDialogues(invalid, error)
             && error == "start_dialogue nao vale dentro de dialogos; use goto <no>"));
    RoomInteractions unknown{{{.trigger = InteractionTrigger::interact("blue_box"),
                               .effects = {InteractionEffect::make("start_dialogue", {"x"})}}}};
    REQUIRE(dialogueReferencesError(unknown, guardDialogues()) == "Dialogo desconhecido: x");

    DialogueChoice locked;
    locked.id = "locked";
    locked.text = "Segredo";
    locked.conditions = {InteractionCondition::make("flag_set", {"never"})};
    game.setDialogues(RoomDialogues{{DialogueNode{
            "lonely", {InteractionEffect::make("say", {"Silencio."})}, {locked}}}});
    REQUIRE(game.startDialogue("lonely"));
    REQUIRE((!game.dialogueActive() && game.scene().floatingText().text() == "Silencio."));
}

// change_room <room> <spawn>: the load places feet and facing there; an unknown spawn refuses it.
TEST_CASE("testRoomSpawns") {
    Room hall = RoomPresets::firstRoom();
    hall.id = "hall";
    hall.spawns = {{"door_left", {120.0F, 520.0F}, Facing::right},
                   {"door_right", {1150.0F, 450.0F}, Facing::left}};
    hall.playerAnimations["idle_left"] = hall.playerAnimations.at("idle");
    REQUIRE(hall.validationError().empty());
    REQUIRE(hall.entryPoint("")->feet.x == hall.playerStart.x);
    REQUIRE(hall.entryPoint("door_right")->facing == Facing::left);
    REQUIRE(!hall.entryPoint("window"));

    Game game(RoomPresets::firstRoom());
    game.setInteractions(RoomInteractions{{{
        .trigger = InteractionTrigger::interact("blue_box"),
        .conditions = {},
        .effects = {InteractionEffect::make("change_room", {"hall", "door_right"})},
    }}});
    std::vector<GameEvent> drained;
    game.drainEvents(drained);
    game.onClick({430.0F, 410.0F});
    for (int i = 0; i < 200 && game.pendingEvents().empty(); ++i) game.tick(0.05F);
    const auto* request = std::get_if<RoomChangeRequested>(&game.pendingEvents().front());
    REQUIRE((request && request->roomId == "hall" && request->spawnId == "door_right"));
    game.drainEvents(drained);

    std::string error;
    const SDL_FPoint before = game.scene().player().feet();
    REQUIRE(!game.applyRoom(hall, true, error, "window"));
    REQUIRE(error == "Spawn desconhecido na sala hall: window");
    REQUIRE((game.scene().room().id == "first" && game.pendingEvents().empty()));
    REQUIRE(near(game.scene().player().feet().x, before.x));

    REQUIRE(game.applyRoom(hall, true, error, "door_right"));
    REQUIRE(near(game.scene().player().feet().x, 1150.0F));
    REQUIRE(near(game.scene().player().feet().y, 450.0F));
    REQUIRE(game.scene().player().facing() == Facing::left);
    const auto& entered = std::get<RoomEntered>(game.pendingEvents().back());
    REQUIRE((entered.roomId == "hall" && entered.spawnId == "door_right"));
    // Standing still: idle_<facing> when the room has it, idle otherwise.
    REQUIRE(playerClipName(game.scene().player(), hall.playerAnimations) == "idle_left");

    // A live edit keeps feet and facing; another load without spawn uses playerStart.
    Room edited = hall;
    edited.playerFarSpeed += 5.0F;
    REQUIRE(game.applyRoom(edited, false, error));
    REQUIRE(game.scene().player().facing() == Facing::left);
    REQUIRE(game.applyRoom(hall, true, error, "door_left"));
    REQUIRE(game.scene().player().facing() == Facing::right);
    REQUIRE(playerClipName(game.scene().player(), hall.playerAnimations) == "idle");
    REQUIRE(game.applyRoom(hall, true, error));
    REQUIRE(near(game.scene().player().feet().x, hall.playerStart.x));
    REQUIRE(game.scene().player().facing() == Facing::down);

    // A spawn on an obstacle falls back to the nearest walkable point, like playerStart.
    Room blocked = hall;
    blocked.spawns = {{"inside_box", {865.0F, 573.0F}, Facing::up}};
    REQUIRE(game.applyRoom(blocked, true, error, "inside_box"));
    REQUIRE(game.scene().navigation().canStandAt(game.scene().player().feet()));

    // Invalid spawns never enter the simulation.
    Room invalid = hall;
    invalid.spawns.push_back({"door_left", {10.0F, 500.0F}, Facing::up});
    REQUIRE(invalid.validationError() == "ID de spawn invalido ou repetido");
    invalid.spawns = {{"far", {hall.width + 5.0F, 500.0F}, Facing::up}};
    REQUIRE(invalid.validationError() == "Spawn fora da sala: far");
}

// GameEvent order: loads, lines, cutscene edges, room requests. Live edits stay silent and a room
// change drops stale requests.
TEST_CASE("testGameEventQueue") {
    Room room = RoomPresets::firstRoom();
    Game game(room);
    std::vector<GameEvent> drained;
    game.drainEvents(drained);
    REQUIRE(drained.size() == 1);
    REQUIRE(std::get<RoomEntered>(drained[0]).roomId == "first");
    REQUIRE(game.pendingEvents().empty());

    std::string error;
    Room edited = room;
    edited.playerFarSpeed += 10.0F;
    REQUIRE(game.applyRoom(edited, false, error));
    REQUIRE(game.pendingEvents().empty()); // live edit: no RoomEntered

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
    REQUIRE((game.inCutscene() && game.scene().floatingText().text() == "B"));
    REQUIRE(!requestedRoom(game));
    game.onClick({200.0F, 600.0F}); // skip B
    game.tick(0.05F);               // change_room runs, the sequence ends, the request goes out
    REQUIRE(!game.inCutscene());

    const auto events = game.pendingEvents();
    REQUIRE(events.size() == 5);
    REQUIRE(std::get<LineSpoken>(events[0]).text == "A");
    REQUIRE(std::holds_alternative<CutsceneStarted>(events[1]));
    REQUIRE(std::get<LineSpoken>(events[2]).text == "B");
    REQUIRE(std::get<RoomChangeRequested>(events[3]).roomId == "gallery");
    REQUIRE(std::holds_alternative<CutsceneEnded>(events[4]));

    // Entering another room before the shell drained the queue drops the old request.
    Room other = RoomPresets::firstRoom();
    other.id = "other";
    game.loadRoom(other);
    REQUIRE(!requestedRoom(game));
    REQUIRE(std::get<RoomEntered>(game.pendingEvents().back()).roomId == "other");

    // drainEvents hands the events over and leaves the queue empty.
    drained.reserve(32);
    game.drainEvents(drained);
    REQUIRE(drained.size() == 5);
    REQUIRE(game.pendingEvents().empty());
}

// A click on a point the player can never reach: a single flood fill picks the nearest
// reachable cell and one A* walks there, even in the largest room the format allows.
TEST_CASE("testNearestReachableInLargeRoom") {
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
    REQUIRE(!route.empty());
    // Nearest interior cell to the target: column 6, row 5 (centre 208, 176).
    REQUIRE((near(route.back().x, 208.0F) && near(route.back().y, 176.0F)));
    for (const SDL_FPoint step : route) REQUIRE(navigation.canStandAt(step));

    // A reachable target still gets the exact point.
    const auto direct = navigation.findPathToNearestReachable(inside, {200.0F, 90.0F});
    REQUIRE((!direct.empty() && near(direct.back().x, 200.0F) && near(direct.back().y, 90.0F)));
}

TEST_CASE("testCameraTargetFor") {
    Camera camera(2000.0F, 1000.0F);

    // Foco no centro da viewport produz offset zero.
    SDL_FPoint target = camera.targetFor({640.0F, 360.0F});
    REQUIRE(near(target.x, 0.0F));
    REQUIRE(near(target.y, 0.0F));

    // Foco livre: o alvo centraliza o pé do jogador na viewport.
    target = camera.targetFor({1000.0F, 600.0F});
    REQUIRE(near(target.x, 360.0F));
    REQUIRE(near(target.y, 240.0F));

    // Limites: offset nunca sai de [0, room - World].
    target = camera.targetFor({1900.0F, 900.0F});
    REQUIRE(near(target.x, 720.0F));
    REQUIRE(near(target.y, 280.0F));
    target = camera.targetFor({-500.0F, -500.0F});
    REQUIRE(near(target.x, 0.0F));
    REQUIRE(near(target.y, 0.0F));

    // targetFor é const: não altera a posição atual da câmera.
    camera.snapTo({1000.0F, 600.0F});
    const SDL_FPoint before = camera.position();
    (void)camera.targetFor({100.0F, 100.0F});
    REQUIRE(near(camera.position().x, before.x));
    REQUIRE(near(camera.position().y, before.y));

    // Foco não finito devolve a posição atual, sem propagar NaN.
    const SDL_FPoint nanTarget = camera.targetFor({std::nanf(""), 10.0F});
    REQUIRE(near(nanTarget.x, camera.position().x));
    REQUIRE(near(nanTarget.y, camera.position().y));
    const SDL_FPoint infTarget = camera.targetFor({10.0F, INFINITY});
    REQUIRE(near(infTarget.x, camera.position().x));
    REQUIRE(near(infTarget.y, camera.position().y));

    // Sala menor que a viewport: a câmera fica travada em (0, 0).
    Camera small(100.0F, 100.0F);
    target = small.targetFor({1200.0F, 700.0F});
    REQUIRE(near(target.x, 0.0F));
    REQUIRE(near(target.y, 0.0F));
}

std::string readAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

TEST_CASE("testSaveRoundTrip") {
    const TempDirectory testDirectory("pointclick-save-tests");
    const std::filesystem::path& directory = testDirectory.path();
    SessionState session;
    session.setFlag("porta_aberta");
    session.setFlag("visitas", 3);
    session.setFlag("apagada");
    session.clearFlag("apagada");
    session.addItem("lanterna"); // inventory order, not alphabetical
    session.addItem("chave");
    session.setObjectVisible("first", "red_box", false);
    session.setObjectSolid("first", "red_box", false);
    session.setObjectSolid("gallery", "porta", true);
    session.setObjectSolid("gallery", "cofre", true);
    session.clearObjectSolidOverride("gallery", "cofre"); // cleared = not saved
    session.markDialogueChoice("gallery", "mesa", "gavetas");
    session.markDialogueChoice("first", "guarda", "oi");

    const SavedGame saved{"gallery", {220.5F, 460.25F}, session.snapshot()};
    REQUIRE((saved.session.flags.size() == 2 && saved.session.flags[0].id == "porta_aberta"));
    REQUIRE(saved.session.items == (std::vector<std::string>{"lanterna", "chave"}));
    REQUIRE(saved.session.objects.size() == 2);
    REQUIRE((saved.session.dialogueChoices.size() == 2
             && saved.session.dialogueChoices[0].roomId == "first"));

    const auto path = directory / "save.slot";
    std::string error;
    REQUIRE(SaveFiles::save(path, saved, error));
    REQUIRE(!std::filesystem::exists(directory / "save.slot.tmp"));
    const std::string firstWrite = readAll(path);
    REQUIRE(firstWrite.find("version = 3\n") != std::string::npos);
    REQUIRE(firstWrite.find("facing = down\n") != std::string::npos);
    REQUIRE(firstWrite.find("object.first.red_box.visible = 0\n") != std::string::npos);
    REQUIRE(firstWrite.find("dialogue = gallery/mesa/gavetas\n") != std::string::npos);
    REQUIRE(firstWrite.find("cofre") == std::string::npos);

    const SaveLoadResult loaded = SaveFiles::load(path);
    REQUIRE((loaded.save && loaded.error.empty()));
    REQUIRE(*loaded.save == saved);
    SessionState restored;
    restored.setFlag("antiga"); // replaced, not merged
    REQUIRE(restored.restore(loaded.save->session, error));
    REQUIRE(restored.snapshot() == session.snapshot());
    REQUIRE((!restored.hasFlag("antiga") && restored.flagValue("visitas") == 3));
    REQUIRE(restored.items() == session.items());
    REQUIRE(!restored.isObjectVisible("first", "red_box"));
    REQUIRE(restored.objectSolidOverride("gallery", "porta") == std::optional<bool>(true));
    REQUIRE(!restored.objectSolidOverride("gallery", "cofre"));
    REQUIRE(restored.dialogueChoiceTaken("gallery", "mesa", "gavetas"));

    // Deterministic: the same progress built in another order writes the same bytes.
    SessionState reordered;
    reordered.markDialogueChoice("first", "guarda", "oi");
    reordered.markDialogueChoice("gallery", "mesa", "gavetas");
    reordered.setObjectSolid("gallery", "porta", true);
    reordered.setObjectSolid("first", "red_box", false);
    reordered.setObjectVisible("first", "red_box", false);
    reordered.setFlag("visitas", 3);
    reordered.setFlag("porta_aberta");
    reordered.addItem("lanterna");
    reordered.addItem("chave");
    REQUIRE(SaveFiles::save(path, {"gallery", {220.5F, 460.25F}, reordered.snapshot()}, error));
    REQUIRE(readAll(path) == firstWrite);
    REQUIRE(SaveFiles::serialize(*loaded.save) == firstWrite);

    // No file is no save and no error.
    const SaveLoadResult missing = SaveFiles::load(directory / "nenhum.slot");
    REQUIRE((!missing.save && missing.error.empty()));
}

// C4: the clip `set_animation` leaves an object in is progress. It survives leaving the room
// and the save (version 3), and comes back on its last frame instead of playing again.
TEST_CASE("testObjectAnimationPersists") {
    const TempDirectory testDirectory("pointclick-save-tests");
    const std::filesystem::path& directory = testDirectory.path();
    Room room = RoomPresets::firstRoom();
    room.id = "hall";
    const SpriteClip closed{.file = "door_closed.png"};
    const SpriteClip open{.file = "door_open.png",
                          .frameWidth = 8,
                          .frameHeight = 8,
                          .frames = 3,
                          .fps = 7.0F,
                          .loop = false};
    const SpriteClip sway{.file = "door_sway.png",
                          .frameWidth = 8,
                          .frameHeight = 8,
                          .frames = 2,
                          .fps = 4.0F,
                          .loop = false};
    SceneObject& door = room.scenery[1]; // red_box
    const std::string doorId = door.id;
    door.animations = {{"idle", closed}, {"open", open}, {"sway", sway}};
    SceneObject& lamp = room.scenery[0];
    const std::string lampId = lamp.id;
    lamp.animations = {{"idle", closed}, {"sway", sway}};
    Room elsewhere = RoomPresets::firstRoom();
    elsewhere.id = "elsewhere";

    const RoomInteractions rules{{
        {.trigger = InteractionTrigger::interact(doorId),
         .conditions = {InteractionCondition::make("flag_not_set", {"door_open"})},
         .effects = {InteractionEffect::make("set_flag", {"door_open"}),
                     InteractionEffect::make("set_solid", {doorId, "0"}),
                     InteractionEffect::make("set_animation", {doorId, "open"}),
                     InteractionEffect::make("play_animation", {lampId, "sway"}),
                     InteractionEffect::make("wait_animation", {doorId})}},
        // Afterwards: a passing play_animation over the saved state.
        {.trigger = InteractionTrigger::interact(doorId),
         .effects = {InteractionEffect::make("play_animation", {doorId, "sway"})}},
    }};
    const auto clickDoor = [&](Game& game) {
        const SDL_FRect bounds = game.scene().room().findObject(doorId)->bounds;
        game.onClick({bounds.x + bounds.w * 0.5F, bounds.y + bounds.h * 0.5F});
        for (int i = 0; i < 300; ++i) game.tick(0.05F);
        REQUIRE(!game.inCutscene());
    };
    const auto shownOpen = [&](const Game& game) {
        const AnimationPlayback* playback = game.scene().objectAnimation(doorId);
        // Already over: last frame on the very first render, nothing for a script to wait on.
        return playback && playback->name() == "open" && playback->finished(open)
               && open.frameAt(playback->interpolatedElapsed(0.0F)) == open.frames - 1
               && game.scene().animationFinished(doorId);
    };

    Game game(room);
    game.setInteractions(rules);
    REQUIRE(game.scene().objectAnimation(doorId)->name() == "idle");
    clickDoor(game);
    REQUIRE((game.session().hasFlag("door_open") && shownOpen(game)));
    REQUIRE(game.session().objectAnimation("hall", doorId) == "open");
    REQUIRE(game.session().objectAnimation("hall", lampId).empty()); // play_animation: not saved
    REQUIRE(game.scene().objectAnimation(lampId)->name() == "sway");

    // Leaving and coming back: the door is open at once; the lamp is back at its initial clip.
    game.loadRoom(elsewhere);
    REQUIRE(game.session().objectAnimation("hall", doorId) == "open");
    game.loadRoom(room);
    REQUIRE(shownOpen(game));
    REQUIRE(game.scene().objectAnimation(lampId)->name() == "idle");
    REQUIRE(!game.scene().isObjectSolid(*game.scene().room().findObject(doorId)));

    // play_animation stays of the moment: it shows now and is gone on the next visit.
    game.setInteractions(rules);
    clickDoor(game);
    REQUIRE(game.scene().objectAnimation(doorId)->name() == "sway");
    REQUIRE(game.session().objectAnimation("hall", doorId) == "open");
    game.loadRoom(elsewhere);
    game.loadRoom(room);
    REQUIRE(shownOpen(game));

    // A live edit of the same room (DebugEditor) keeps what is playing, as before.
    std::string error;
    REQUIRE(game.playObjectAnimation(doorId, "sway", true, error));
    REQUIRE(game.applyRoom(room, false, error));
    REQUIRE(game.scene().objectAnimation(doorId)->name() == "sway");

    // Save version 3: deterministic round-trip of the new key.
    const SavedGame saved = game.savedGame();
    const std::string text = SaveFiles::serialize(saved);
    REQUIRE(text.find("version = 3\n") != std::string::npos);
    REQUIRE(text.find("object.hall." + doorId + ".solid = 0\nobject.hall." + doorId
                      + ".animation = open\n")
            != std::string::npos);
    const SaveLoadResult parsed = SaveFiles::parse(text);
    REQUIRE((parsed.save && *parsed.save == saved));
    REQUIRE(SaveFiles::serialize(*parsed.save) == text);
    const auto path = directory / "animation.slot";
    REQUIRE(SaveFiles::save(path, saved, error));
    const SaveLoadResult loaded = SaveFiles::load(path);
    REQUIRE((loaded.save && *loaded.save == saved && readAll(path) == text));
    // An override with nothing but the clip is a state of its own.
    SessionState onlyClip;
    onlyClip.setObjectAnimation("hall", "window", "open");
    const SessionSnapshot clipSnapshot = onlyClip.snapshot();
    REQUIRE((clipSnapshot.objects.size() == 1 && clipSnapshot.validationError().empty()));
    REQUIRE(clipSnapshot.objects[0].animation == std::optional<std::string>("open"));
    REQUIRE(SaveFiles::parse(SaveFiles::serialize({"hall", {1.0F, 2.0F}, clipSnapshot}))
                    .save->session
            == clipSnapshot);

    // Loading the save: the door shows open from the first frame, without animating again.
    SessionState restored;
    REQUIRE(restored.restore(loaded.save->session, error));
    Game resumed(room, std::move(restored));
    REQUIRE((shownOpen(resumed) && resumed.session().hasFlag("door_open")));
    REQUIRE(resumed.savedGame().session == saved.session);

    // Version 2 files still load (no animation); the new key is refused in them by version.
    const std::string v2 = "version = 2\nroom = hall\nfeet = 10, 20\nfacing = up\n"
                           "flag.door_open = 1\nobject.hall."
                           + doorId + ".solid = 0\n";
    const SaveLoadResult older = SaveFiles::parse(v2);
    REQUIRE((older.save && older.save->facing == Facing::up));
    REQUIRE(older.save->session.objects.size() == 1);
    REQUIRE(!older.save->session.objects[0].animation);
    REQUIRE(older.save->session.objects[0].solidOverride == std::optional<bool>(false));
    REQUIRE(SaveFiles::parse(v2 + "object.hall." + doorId + ".animation = open\n").error
            == "Linha 7: Chave ou valor invalido: object.hall." + doorId + ".animation");
    const std::string v3 = "version = 3\nroom = hall\nfeet = 10, 20\n";
    REQUIRE(SaveFiles::parse(v3 + "object.hall.door.animation = a b\n").error
            == "Linha 4: Chave ou valor invalido: object.hall.door.animation");
    REQUIRE(SaveFiles::parse(v3 + "object.hall.door.animation = open\n").save);

    // A clip (or object) the room no longer has is harmless: the initial animation shows.
    SessionState stale;
    stale.setObjectAnimation("hall", doorId, "melted");
    stale.setObjectAnimation("hall", "removed_object", "open");
    stale.setObjectAnimation("hall", lampId, "not valid"); // refused: not an identifier
    REQUIRE(stale.objectAnimation("hall", lampId).empty());
    Game tolerant(room, std::move(stale));
    REQUIRE(tolerant.scene().objectAnimation(doorId)->name() == "idle");
    REQUIRE(tolerant.scene().objectAnimation(lampId)->name() == "idle");
    SessionSnapshot invalid = tolerant.session().snapshot();
    invalid.objects[0].animation = "a b";
    SessionState untouched;
    REQUIRE((!untouched.restore(invalid, error)
             && error.starts_with("Animacao invalida em hall.")));
}

TEST_CASE("testSaveRestoresPuzzleProgress") {
    const TempDirectory testDirectory("pointclick-save-tests");
    const std::filesystem::path& directory = testDirectory.path();
    const Room room = RoomPresets::firstRoom();
    const SDL_FPoint behindLock{865.0F, 573.0F}; // inside red_box's collision footprint
    Game game(room);
    game.setInteractions(RoomPresets::firstRoomInteractions());
    REQUIRE(!game.scene().navigation().canStandAt(behindLock));

    game.onClick({430.0F, 410.0F}); // blue_box: the key
    for (int i = 0; i < 300 && !game.session().hasItem("brass_key"); ++i) game.tick(0.05F);
    REQUIRE(game.session().hasItem("brass_key"));
    REQUIRE(game.useItemOnObject("brass_key", "red_box"));
    for (int i = 0; i < 300 && !game.session().hasFlag("red_box_unlocked"); ++i) game.tick(0.05F);
    for (int i = 0; i < 300 && (game.inCutscene() || game.scene().player().moving()); ++i) {
        game.tick(0.05F);
    }
    REQUIRE((game.session().hasFlag("red_box_unlocked") && !game.session().hasItem("brass_key")));
    REQUIRE(game.scene().navigation().canStandAt(behindLock));

    const auto path = directory / "puzzle.slot";
    std::string error;
    const SavedGame saved = game.savedGame();
    REQUIRE(saved.roomId == "first");
    REQUIRE(SaveFiles::save(path, saved, error));

    // A new process: the progress goes in before the room, then the feet go back.
    const SaveLoadResult loaded = SaveFiles::load(path);
    REQUIRE(loaded.save);
    SessionState progress;
    REQUIRE(progress.restore(loaded.save->session, error));
    Game resumed(room, std::move(progress));
    resumed.setInteractions(RoomPresets::firstRoomInteractions());
    const SDL_FPoint feet = resumed.placePlayerNear(loaded.save->feet);
    REQUIRE((near(feet.x, saved.feet.x) && near(feet.y, saved.feet.y)));
    REQUIRE(resumed.session().hasFlag("found_brass_key"));
    REQUIRE(resumed.session().hasFlag("red_box_unlocked"));
    REQUIRE(!resumed.session().hasItem("brass_key"));
    REQUIRE(!resumed.scene().isObjectSolid(resumed.scene().room().scenery[1]));
    REQUIRE(resumed.scene().navigation().canStandAt(behindLock));
    // The open box lets the route go straight through where it used to be.
    std::string ignored;
    REQUIRE(resumed.teleportPlayer({760.0F, 573.0F}, ignored));
    resumed.onClick({970.0F, 573.0F});
    for (int i = 0; i < 200 && resumed.scene().player().moving(); ++i) resumed.tick(0.05F);
    REQUIRE(near(resumed.scene().player().feet().x, 970.0F));
    REQUIRE(resumed.savedGame().session == saved.session);

    // Feet saved on a spot that is blocked now fall back to the nearest walkable point.
    Game locked(room);
    const SDL_FPoint fallback = locked.placePlayerNear(behindLock);
    REQUIRE(locked.scene().navigation().canStandAt(fallback));
    REQUIRE((!near(fallback.x, behindLock.x) || !near(fallback.y, behindLock.y)));
}

// Version 2 adds the facing; version 1 files keep loading (facing down) and cannot carry it.
TEST_CASE("testSaveFacingAndOlderVersions") {
    const TempDirectory testDirectory("pointclick-save-tests");
    const std::filesystem::path& directory = testDirectory.path();
    SavedGame saved{"gallery", {300.0F, 500.0F}, {}};
    saved.facing = Facing::left;
    const std::string text = SaveFiles::serialize(saved);
    REQUIRE(text.find("version = 3\nroom = gallery\nfeet = 300, 500\nfacing = left\n")
            != std::string::npos);
    const SaveLoadResult parsed = SaveFiles::parse(text);
    REQUIRE((parsed.save && *parsed.save == saved));

    const SaveLoadResult older = SaveFiles::parse("version = 1\nroom = first\nfeet = 10, 20\n"
                                                  "item = chave\n");
    REQUIRE((older.save && older.save->facing == Facing::down));
    REQUIRE(older.save->session.items == std::vector<std::string>{"chave"});
    REQUIRE(!SaveFiles::parse("version = 1\nroom = first\nfeet = 10, 20\nfacing = up\n").save);
    REQUIRE(SaveFiles::parse("version = 2\nroom = first\nfeet = 10, 20\n").save); // optional
    REQUIRE(SaveFiles::parse("version = 2\nroom = first\nfeet = 1, 2\nfacing = norte\n").error
            == "Linha 4: Chave ou valor invalido: facing");

    // The resumed player looks where it looked; saving it again gives the same bytes.
    Game game(RoomPresets::firstRoom());
    game.onClick({300.0F, 360.0F}); // walk left
    for (int i = 0; i < 200 && game.scene().player().moving(); ++i) game.tick(0.05F);
    REQUIRE(game.scene().player().facing() == Facing::left);
    const auto path = directory / "facing.slot";
    std::string error;
    REQUIRE(SaveFiles::save(path, game.savedGame(), error));
    const SaveLoadResult loaded = SaveFiles::load(path);
    REQUIRE((loaded.save && loaded.save->facing == Facing::left));
    Game resumed(RoomPresets::firstRoom());
    REQUIRE(resumed.scene().player().facing() == Facing::down);
    (void)resumed.placePlayerNear(loaded.save->feet, loaded.save->facing);
    REQUIRE(resumed.scene().player().facing() == Facing::left);
    REQUIRE(resumed.savedGame() == *loaded.save);

    // A save edited by hand in a Windows editor (UTF-8 BOM, CRLF) still loads.
    const auto edited = directory / "bom.slot";
    std::ofstream(edited, std::ios::binary)
            << "\xEF\xBB\xBF" << "version = 2\r\nroom = gallery\r\nfeet = 300, 500\r\n";
    const SaveLoadResult fromEditor = SaveFiles::load(edited);
    REQUIRE((fromEditor.save && fromEditor.save->roomId == "gallery"));
}

// Floats are written as the shortest plain decimal that reads back the same, not with
// max_digits10 noise ("53.3333321"), and never follow the process locale.
TEST_CASE("testShortestNumbersInSave") {
    REQUIRE(TextFormat::number(300.0F) == "300");
    REQUIRE(TextFormat::number(0.1F) == "0.1");
    REQUIRE(TextFormat::number(-0.25F) == "-0.25");
    REQUIRE(TextFormat::number(1150.5F) == "1150.5");
    REQUIRE(TextFormat::number(53.333333F) == "53.333332");
    REQUIRE(TextFormat::number(100000.0F) == "100000"); // no exponent
    REQUIRE(TextFormat::number(0.0F) == "0");

    const float samples[]{53.333333F, 0.1F, 1.0F / 3.0F, 16777217.0F, 1234.5678F, 0.000123F,
                          -719.99994F, std::numeric_limits<float>::max(),
                          std::numeric_limits<float>::min(),
                          -std::numeric_limits<float>::denorm_min()};
    for (const float sample : samples) {
        const std::string text = TextFormat::number(sample);
        REQUIRE((text.find('e') == std::string::npos && text.size() < 64));
        float back = 0.0F;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), back);
        REQUIRE((result.ec == std::errc{} && result.ptr == text.data() + text.size()));
        REQUIRE(back == sample);
    }

    SavedGame saved{"gallery", {53.333333F, 0.1F}, {}};
    const std::string text = SaveFiles::serialize(saved);
    REQUIRE(text.find("feet = 53.333332, 0.1\n") != std::string::npos);
    const SaveLoadResult parsed = SaveFiles::parse(text);
    REQUIRE((parsed.save && *parsed.save == saved));

    // A decimal-comma locale changes nothing in the file.
    const std::locale previous;
    const std::string previousC = std::setlocale(LC_ALL, nullptr);
    for (const char* name : {"pt_BR.UTF-8", "de_DE.UTF-8"}) {
        try {
            std::locale::global(std::locale(name));
        } catch (const std::runtime_error&) {
            continue; // not installed here
        }
        REQUIRE(SaveFiles::serialize(saved) == text);
        const SaveLoadResult localized = SaveFiles::parse(text);
        REQUIRE((localized.save && *localized.save == saved));
    }
    std::locale::global(previous);
    std::setlocale(LC_ALL, previousC.c_str());
}

TEST_CASE("testBadSaveFiles") {
    const TempDirectory testDirectory("pointclick-save-tests");
    const std::filesystem::path& directory = testDirectory.path();
    const auto errorOf = [](const std::string_view text) {
        const SaveLoadResult result = SaveFiles::parse(text);
        REQUIRE(!result.save);
        return result.error;
    };
    const std::string header = "version = 1\nroom = first\nfeet = 10, 20\n";
    REQUIRE(SaveFiles::parse(header + "# ok\nflag.a = -2\n").save);
    REQUIRE(errorOf("") == "Save vazio ou sem version");
    REQUIRE(errorOf("# so comentario\n") == "Save vazio ou sem version");
    REQUIRE(errorOf("version = 4\nroom = first\n") == "Linha 1: Versao de save desconhecida: 4");
    REQUIRE(errorOf("version = 0\n") == "Linha 1: Versao de save desconhecida: 0");
    REQUIRE(errorOf("room = first\nversion = 1\n") == "Linha 1: A primeira chave deve ser version");
    REQUIRE(errorOf(header + "camera = 1, 2\n") == "Linha 4: Chave desconhecida: camera");
    REQUIRE(errorOf(header + "item = chave!\n") == "Linha 4: Chave ou valor invalido: item");
    REQUIRE(errorOf(header + "item = chave\nitem = chave\n") == "Linha 5: Item repetido: chave");
    REQUIRE(errorOf(header + "flag.a = 1\nflag.a = 2\n") == "Linha 5: Chave repetida: flag.a");
    REQUIRE(errorOf(header + "flag.a = 0\n") == "Linha 4: Chave ou valor invalido: flag.a");
    REQUIRE(errorOf(header + "flag.a b = 1\n") == "Linha 4: Chave ou valor invalido: flag.a b");
    REQUIRE(errorOf(header + "object.first.box.visible = 2\n")
            == "Linha 4: Chave ou valor invalido: object.first.box.visible");
    REQUIRE(errorOf(header + "object.first.box.color = 1\n")
            == "Linha 4: Chave ou valor invalido: object.first.box.color");
    REQUIRE(errorOf(header + "dialogue = a/b\n") == "Linha 4: Chave ou valor invalido: dialogue");
    REQUIRE(errorOf(header + "item chave\n") == "Linha 4: Esperado chave = valor");
    REQUIRE(errorOf("version = 1\nroom = sala x\n") == "Linha 2: Chave ou valor invalido: room");
    REQUIRE(errorOf("version = 1\nroom = first\nfeet = nan, 1\n")
            == "Linha 3: Chave ou valor invalido: feet");
    REQUIRE(errorOf("version = 1\nfeet = 1, 2\n") == "Falta a chave room");
    REQUIRE(errorOf("version = 1\nroom = first\n") == "Falta a chave feet");

    // An invalid snapshot never touches the progress in place.
    SessionState session;
    session.addItem("chave");
    SessionSnapshot invalid;
    invalid.items = {"ok", "nao ok"};
    std::string error;
    REQUIRE((!session.restore(invalid, error) && error == "Item invalido: nao ok"));
    REQUIRE((session.hasItem("chave") && !session.hasItem("ok")));

    // Boot: missing = new game quietly; refused = new game, the file kept as .bad (or left
    // alone under --no-save); a room that no longer loads keeps the progress in the start room.
    const auto rooms = directory / "save-rooms";
    std::filesystem::create_directories(rooms);
    REQUIRE(RoomFiles::save(rooms / "first.room", RoomPresets::firstRoom(), error));
    RoomLibrary library;
    REQUIRE(library.open(rooms, error));
    const auto slot = directory / "boot.slot";
    const auto bad = directory / "boot.slot.bad";
    for (const RefusedSave mode : {RefusedSave::quarantine, RefusedSave::keep}) {
        ResumeResult none = SaveFiles::resume(slot, library, mode);
        REQUIRE((!none.save && none.error.empty()));
        REQUIRE((!std::filesystem::exists(slot) && !std::filesystem::exists(bad)));
    }

    std::ofstream(slot) << "version = 2\nroom = sumida\nfeet = 10, 20\nfacing = up\n"
                           "flag.porta = 1\nitem = chave\nobject.sumida.cofre.visible = 0\n";
    ResumeResult moved = SaveFiles::resume(slot, library, RefusedSave::quarantine);
    REQUIRE((moved.save && moved.bundle && moved.error.empty()));
    REQUIRE(moved.warning.starts_with("Sala sumida: "));
    REQUIRE((moved.bundle->room.id == "first" && moved.save->roomId == "first"));
    const Room start = RoomPresets::firstRoom();
    REQUIRE((moved.save->feet.x == start.playerStart.x
             && moved.save->feet.y == start.playerStart.y));
    REQUIRE(moved.save->facing == Facing::down);
    REQUIRE(moved.save->session.items == std::vector<std::string>{"chave"});
    REQUIRE((moved.save->session.flags.size() == 1 && moved.save->session.objects.size() == 1));
    REQUIRE((std::filesystem::exists(slot) && !std::filesystem::exists(bad)));

    // D11: with RefusedSave::keep (--no-save) a refused save is reported the same way and stays
    // on disk byte for byte, with no .bad beside it; both ways of being refused.
    const std::string lostRoom = "version = 1\nroom = sumida\nfeet = 10, 20\n";
    std::ofstream(slot) << lostRoom;
    ResumeResult keptRoom = SaveFiles::resume(slot, library, RefusedSave::keep, "inexistente.room");
    REQUIRE((!keptRoom.save && !keptRoom.bundle && keptRoom.error.starts_with("Sala sumida: ")));
    REQUIRE((readAll(slot) == lostRoom && !std::filesystem::exists(bad)));
    const std::string corrupt = "version = 9\n# nao mexa\n";
    std::ofstream(slot) << corrupt;
    ResumeResult kept = SaveFiles::resume(slot, library, RefusedSave::keep);
    REQUIRE((!kept.save && !kept.bundle && kept.warning.empty()));
    REQUIRE(kept.error == "Linha 1: Versao de save desconhecida: 9");
    REQUIRE((readAll(slot) == corrupt && !std::filesystem::exists(bad)));
    REQUIRE(!std::filesystem::exists(directory / "boot.slot.tmp"));

    // Refused only when the start room does not load either.
    std::ofstream(slot) << lostRoom;
    ResumeResult gone =
            SaveFiles::resume(slot, library, RefusedSave::quarantine, "inexistente.room");
    REQUIRE((!gone.save && !gone.bundle && gone.warning.empty()));
    REQUIRE((gone.error.starts_with("Sala sumida: ")
             && gone.error.find("; inexistente.room: ") != std::string::npos));
    REQUIRE(!std::filesystem::exists(slot));
    REQUIRE(readAll(bad).find("room = sumida") != std::string::npos);

    std::ofstream(slot) << "version = 9\n";
    ResumeResult future = SaveFiles::resume(slot, library, RefusedSave::quarantine);
    REQUIRE((!future.save && future.error == "Linha 1: Versao de save desconhecida: 9"));
    REQUIRE(!std::filesystem::exists(slot));
    REQUIRE(readAll(bad) == "version = 9\n"); // the newest refused one
    REQUIRE((future.setAside && gone.setAside && !kept.setAside));

    // A rename that fails (a non-empty folder sits where .bad goes) is reported, not hidden.
    std::filesystem::remove(bad);
    std::filesystem::create_directory(bad);
    std::ofstream(bad / "keep.txt") << "x";
    std::ofstream(slot) << "version = 9\n";
    ResumeResult stuck = SaveFiles::resume(slot, library, RefusedSave::quarantine);
    REQUIRE((!stuck.save && !stuck.setAside));
    REQUIRE(stuck.error.starts_with("Linha 1: Versao de save desconhecida: 9 ("));
    REQUIRE(readAll(slot) == "version = 9\n"); // still there
    std::filesystem::remove_all(bad);
    std::ofstream(bad) << "version = 9\n"; // as the next checks expect it

    // A good save is never touched, in either mode (the older .bad stays too).
    const std::string good = "version = 1\nroom = first\nfeet = 300, 600\nitem = chave\n";
    std::ofstream(slot) << good;
    for (const RefusedSave mode : {RefusedSave::quarantine, RefusedSave::keep}) {
        ResumeResult ok = SaveFiles::resume(slot, library, mode);
        REQUIRE((ok.save && ok.bundle && ok.error.empty()));
        REQUIRE((ok.bundle->room.id == "first" && ok.save->session.items.size() == 1));
        REQUIRE((readAll(slot) == good && readAll(bad) == "version = 9\n"));
    }
}

TEST_CASE("testAtomicSaveFailureKeepsPrevious") {
    const TempDirectory testDirectory("pointclick-save-tests");
    const std::filesystem::path& directory = testDirectory.path();
    const auto path = directory / "atomic.slot";
    std::string error;
    SavedGame saved{"first", {100.0F, 600.0F}, {}};
    saved.session.items = {"chave"};
    REQUIRE(SaveFiles::save(path, saved, error));
    const std::string previous = readAll(path);

    // Missing folder: the write fails and creates nothing.
    REQUIRE(!SaveFiles::save(directory / "nao-existe" / "atomic.slot", saved, error));
    REQUIRE(!error.empty());
    REQUIRE(!std::filesystem::exists(directory / "nao-existe"));

    // The temporary cannot be written (a folder sits on its name): the old save survives.
    std::filesystem::create_directory(directory / "atomic.slot.tmp");
    saved.session.items = {"outra"};
    error.clear();
    REQUIRE((!SaveFiles::save(path, saved, error) && !error.empty()));
    REQUIRE(readAll(path) == previous);
    std::filesystem::remove(directory / "atomic.slot.tmp");

    // Invalid data is refused before any write.
    saved.session.items = {"nao ok"};
    REQUIRE((!SaveFiles::save(path, saved, error) && error == "Item invalido: nao ok"));
    REQUIRE(readAll(path) == previous);
}

TEST_CASE("testAutosavePolicy") {
    Game game(RoomPresets::firstRoom());
    game.setInteractions(RoomInteractions{{{
        .trigger = InteractionTrigger::interact("blue_box"),
        .conditions = {},
        .effects = {
            InteractionEffect::make("say", {"Pesada."}), // blocking: not the last step
            InteractionEffect::make("set_flag", {"looked"}),
        },
    }}});

    AutosavePolicy autosave;
    // WILL and DID background are one trip: one write.
    const SavedGame* first = autosave.enterBackground(game);
    REQUIRE((first && first->roomId == "first"));
    autosave.markWritten();
    REQUIRE(!autosave.enterBackground(game));
    REQUIRE(autosave.inBackground());
    // Back and away again with nothing new: the file already has it.
    autosave.enterForeground();
    REQUIRE(!autosave.enterBackground(game));
    REQUIRE(!autosave.quit(game));

    // A failed write is retried on the next trip, still once per trip.
    autosave.enterForeground();
    game.changeSession([](SessionState& session) { session.setFlag("progresso"); });
    REQUIRE(autosave.enterBackground(game));
    REQUIRE(!autosave.enterBackground(game)); // DID after WILL, the write failed
    autosave.enterForeground();
    REQUIRE(autosave.enterBackground(game));
    autosave.markWritten();
    autosave.enterForeground();

    // Mid-cutscene the snapshot stays the last stable one.
    game.onClick({430.0F, 410.0F});
    for (int i = 0; i < 300 && !game.inCutscene(); ++i) game.tick(0.05F);
    REQUIRE(game.inCutscene());
    game.changeSession([](SessionState& session) { session.setFlag("meio_da_cena"); });
    autosave.capture(game);
    REQUIRE(!autosave.enterBackground(game)); // nothing stable that is not on disk yet
    const auto& stable = autosave.stable()->session.flags;
    REQUIRE(std::none_of(stable.begin(), stable.end(),
                         [](const auto& flag) { return flag.id == "meio_da_cena"; }));
    autosave.enterForeground();

    // Once the sequence ends, quitting writes the finished state.
    game.onClick({200.0F, 600.0F}); // skip the line
    for (int i = 0; i < 20 && game.inCutscene(); ++i) game.tick(0.05F);
    REQUIRE((!game.inCutscene() && game.session().hasFlag("looked")));
    const SavedGame* atQuit = autosave.quit(game);
    REQUIRE((atQuit && atQuit->session == game.session().snapshot()));
    autosave.markWritten();
    REQUIRE(!autosave.quit(game)); // SDL_EVENT_QUIT then TERMINATING

    // Entering a room is a checkpoint: written when it differs from the file, so a crash costs
    // at most the current room; never the half-done state of a cutscene.
    REQUIRE(!autosave.checkpoint(game)); // the file already has this
    game.changeSession([](SessionState& session) { session.setFlag("outra_sala"); });
    const SavedGame* entered = autosave.checkpoint(game);
    REQUIRE((entered && entered->session == game.session().snapshot()));
    REQUIRE(autosave.checkpoint(game)); // the write failed: still pending
    autosave.markWritten();
    REQUIRE(!autosave.checkpoint(game));
    game.onClick({430.0F, 410.0F});
    for (int i = 0; i < 300 && !game.inCutscene(); ++i) game.tick(0.05F);
    REQUIRE(game.inCutscene());
    game.changeSession([](SessionState& session) { session.setFlag("no_meio"); });
    REQUIRE(!autosave.checkpoint(game));
}
