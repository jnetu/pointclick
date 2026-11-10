#include "support/RoomPresets.hpp"
#include "content/RoomFiles.hpp"
#include "content/RoomLibrary.hpp"
#include "content/SaveFiles.hpp"
#include "content/TextFormat.hpp"
#include "game/Autosave.hpp"
#include "game/Camera.hpp"
#include "game/Game.hpp"
#include "game/Navigation.hpp"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <chrono>
#include <clocale>
#include <filesystem>
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
    // start walking, and not trigger the effect before arrival.
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
    game.changeSession([](SessionState& session) { session.clearFlag("inspected_statue"); });
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

    // 7. A few pixels off that point but within reach: the interaction fires where the player
    // stands, with no shuffle to the centre of the navigation cell first.
    const SDL_FPoint arrived = game.scene().player().feet();
    std::string nudgeError;
    assert(game.teleportPlayer({arrived.x + 9.0F, arrived.y + 5.0F}, nudgeError));
    const SDL_FPoint nudged = game.scene().player().feet();
    assert(!near(nudged.x, arrived.x));
    game.changeSession([](SessionState& session) { session.clearFlag("saw_painting"); });
    game.onClick({465.0F, 175.0F});
    assert(!game.scene().player().moving());
    assert(!game.scene().pendingInteraction().has_value());
    assert(game.session().hasFlag("saw_painting"));
    assert(near(game.scene().player().feet().x, nudged.x));
    assert(near(game.scene().player().feet().y, nudged.y));

    // 8. Farther than one navigation cell, the player still walks up to the object first.
    assert(game.teleportPlayer({arrived.x + 60.0F, arrived.y + 40.0F}, nudgeError));
    game.changeSession([](SessionState& session) { session.clearFlag("saw_painting"); });
    game.onClick({465.0F, 175.0F});
    assert(game.scene().player().moving());
    assert(game.scene().pendingInteraction().has_value());
    assert(!game.session().hasFlag("saw_painting"));
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

// Progress changed from outside a script goes through Game::changeSession, which rebuilds
// navigation when an object of the room starts or stops blocking the floor.
void testSessionCommandKeepsNavigation() {
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
    assert(!game.scene().navigation().canStandAt(crateCenter));
    assert(!game.teleportPlayer(crateCenter, error));

    // A change that leaves collision alone keeps navigation and the walk in progress.
    game.onClick({400.0F, 600.0F});
    assert(game.scene().player().moving());
    game.changeSession([](SessionState& session) {
        session.setFlag("lever_pulled");
        session.addItem("crowbar");
    });
    assert(game.session().hasFlag("lever_pulled") && game.session().hasItem("crowbar"));
    assert(game.scene().player().moving());
    assert(!game.scene().navigation().canStandAt(crateCenter));

    // Collision turned off: the cells are free at once, not only isObjectSolid.
    game.changeSession([&](SessionState& session) {
        session.setObjectSolid(roomId, "supply_crate", false);
    });
    assert(!game.scene().isObjectSolid(crate));
    assert(game.scene().navigation().canStandAt(crateCenter));
    assert(game.teleportPlayer(crateCenter, error));

    // Solid again under the player's feet: the cells block and the player is moved out.
    game.changeSession([&](SessionState& session) {
        session.setObjectSolid(roomId, "supply_crate", true);
    });
    assert(!game.scene().navigation().canStandAt(crateCenter));
    const SDL_FPoint pushed = game.scene().player().feet();
    assert(!SDL_PointInRectFloat(&pushed, &footprint));
    assert(game.scene().navigation().canStandAt(pushed));

    // Hiding frees the floor too; wiping the progress brings the authored collision back.
    game.changeSession([&](SessionState& session) {
        session.setObjectVisible(roomId, "supply_crate", false);
    });
    assert(game.scene().navigation().canStandAt(crateCenter));
    game.changeSession([](SessionState& session) { session.reset(); });
    assert(game.scene().isObjectSolid(crate));
    assert(!game.scene().navigation().canStandAt(crateCenter));

    // An object of another room with the same id is not this room's collision.
    game.changeSession([](SessionState& session) {
        session.setObjectSolid("another_room", "supply_crate", false);
    });
    assert(!game.scene().navigation().canStandAt(crateCenter));
}

void testConditionalNpcPuzzleAndRoomPersistence() {
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

    // 4. Talk to guard with silver_key and talked_to_guard: key consumed, gate_open set,
    // iron_gate plays "open" and becomes passable.
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
    assert(stillGame.session().hasFlag("still_done"));
    assert(!stillGame.inCutscene());
    assert(stillGame.scene().objectAnimation("red_box")->name() == "opened");

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
    assert(requestedRoom(talker) && *requestedRoom(talker) == "gallery");
    assert(talker.scene().floatingText().isVisible());
    talker.loadRoom(gallery);
    assert(!talker.scene().floatingText().isVisible());
    // A live edit of the same room (resetPlayer = false) keeps the line being read.
    Game editing(RoomPresets::firstRoom());
    editing.setInteractions(RoomPresets::firstRoomInteractions());
    editing.onClick({430.0F, 410.0F});
    for (int i = 0; i < 200 && !editing.scene().floatingText().isVisible(); ++i) editing.tick(0.05F);
    assert(editing.scene().floatingText().isVisible());
    std::string editError;
    assert(editing.applyRoom(RoomPresets::firstRoom(), false, editError));
    assert(editing.scene().floatingText().isVisible());
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
    game.changeSession([](SessionState& session) { session.addItem("brass_key"); });
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

// change_room <room> <spawn>: the load places feet and facing there; an unknown spawn refuses it.
void testRoomSpawns() {
    Room hall = RoomPresets::firstRoom();
    hall.id = "hall";
    hall.spawns = {{"door_left", {120.0F, 520.0F}, Facing::right},
                   {"door_right", {1150.0F, 450.0F}, Facing::left}};
    hall.playerAnimations["idle_left"] = hall.playerAnimations.at("idle");
    assert(hall.validationError().empty());
    assert(hall.entryPoint("")->feet.x == hall.playerStart.x);
    assert(hall.entryPoint("door_right")->facing == Facing::left);
    assert(!hall.entryPoint("window"));

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
    assert(request && request->roomId == "hall" && request->spawnId == "door_right");
    game.drainEvents(drained);

    std::string error;
    const SDL_FPoint before = game.scene().player().feet();
    assert(!game.applyRoom(hall, true, error, "window"));
    assert(error == "Spawn desconhecido na sala hall: window");
    assert(game.scene().room().id == "first" && game.pendingEvents().empty());
    assert(near(game.scene().player().feet().x, before.x));

    assert(game.applyRoom(hall, true, error, "door_right"));
    assert(near(game.scene().player().feet().x, 1150.0F));
    assert(near(game.scene().player().feet().y, 450.0F));
    assert(game.scene().player().facing() == Facing::left);
    const auto& entered = std::get<RoomEntered>(game.pendingEvents().back());
    assert(entered.roomId == "hall" && entered.spawnId == "door_right");
    // Standing still: idle_<facing> when the room has it, idle otherwise.
    assert(playerClipName(game.scene().player(), hall.playerAnimations) == "idle_left");

    // A live edit keeps feet and facing; another load without spawn uses playerStart.
    Room edited = hall;
    edited.playerFarSpeed += 5.0F;
    assert(game.applyRoom(edited, false, error));
    assert(game.scene().player().facing() == Facing::left);
    assert(game.applyRoom(hall, true, error, "door_left"));
    assert(game.scene().player().facing() == Facing::right);
    assert(playerClipName(game.scene().player(), hall.playerAnimations) == "idle");
    assert(game.applyRoom(hall, true, error));
    assert(near(game.scene().player().feet().x, hall.playerStart.x));
    assert(game.scene().player().facing() == Facing::down);

    // A spawn on an obstacle falls back to the nearest walkable point, like playerStart.
    Room blocked = hall;
    blocked.spawns = {{"inside_box", {865.0F, 573.0F}, Facing::up}};
    assert(game.applyRoom(blocked, true, error, "inside_box"));
    assert(game.scene().navigation().canStandAt(game.scene().player().feet()));

    // Invalid spawns never enter the simulation.
    Room invalid = hall;
    invalid.spawns.push_back({"door_left", {10.0F, 500.0F}, Facing::up});
    assert(invalid.validationError() == "ID de spawn invalido ou repetido");
    invalid.spawns = {{"far", {hall.width + 5.0F, 500.0F}, Facing::up}};
    assert(invalid.validationError() == "Spawn fora da sala: far");
}

// GameEvent order: loads, lines, cutscene edges, room requests. Live edits stay silent and a room
// change drops stale requests.
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

std::string readAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

void testSaveRoundTrip(const std::filesystem::path& directory) {
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
    assert(saved.session.flags.size() == 2 && saved.session.flags[0].id == "porta_aberta");
    assert(saved.session.items == (std::vector<std::string>{"lanterna", "chave"}));
    assert(saved.session.objects.size() == 2);
    assert(saved.session.dialogueChoices.size() == 2
           && saved.session.dialogueChoices[0].roomId == "first");

    const auto path = directory / "save.slot";
    std::string error;
    assert(SaveFiles::save(path, saved, error));
    assert(!std::filesystem::exists(directory / "save.slot.tmp"));
    const std::string firstWrite = readAll(path);
    assert(firstWrite.find("version = 2\n") != std::string::npos);
    assert(firstWrite.find("facing = down\n") != std::string::npos);
    assert(firstWrite.find("object.first.red_box.visible = 0\n") != std::string::npos);
    assert(firstWrite.find("dialogue = gallery/mesa/gavetas\n") != std::string::npos);
    assert(firstWrite.find("cofre") == std::string::npos);

    const SaveLoadResult loaded = SaveFiles::load(path);
    assert(loaded.save && loaded.error.empty());
    assert(*loaded.save == saved);
    SessionState restored;
    restored.setFlag("antiga"); // replaced, not merged
    assert(restored.restore(loaded.save->session, error));
    assert(restored.snapshot() == session.snapshot());
    assert(!restored.hasFlag("antiga") && restored.flagValue("visitas") == 3);
    assert(restored.items() == session.items());
    assert(!restored.isObjectVisible("first", "red_box"));
    assert(restored.objectSolidOverride("gallery", "porta") == std::optional<bool>(true));
    assert(!restored.objectSolidOverride("gallery", "cofre"));
    assert(restored.dialogueChoiceTaken("gallery", "mesa", "gavetas"));

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
    assert(SaveFiles::save(path, {"gallery", {220.5F, 460.25F}, reordered.snapshot()}, error));
    assert(readAll(path) == firstWrite);
    assert(SaveFiles::serialize(*loaded.save) == firstWrite);

    // No file is no save and no error.
    const SaveLoadResult missing = SaveFiles::load(directory / "nenhum.slot");
    assert(!missing.save && missing.error.empty());
}

void testSaveRestoresPuzzleProgress(const std::filesystem::path& directory) {
    const Room room = RoomPresets::firstRoom();
    const SDL_FPoint behindLock{865.0F, 573.0F}; // inside red_box's collision footprint
    Game game(room);
    game.setInteractions(RoomPresets::firstRoomInteractions());
    assert(!game.scene().navigation().canStandAt(behindLock));

    game.onClick({430.0F, 410.0F}); // blue_box: the key
    for (int i = 0; i < 300 && !game.session().hasItem("brass_key"); ++i) game.tick(0.05F);
    assert(game.session().hasItem("brass_key"));
    assert(game.useItemOnObject("brass_key", "red_box"));
    for (int i = 0; i < 300 && !game.session().hasFlag("red_box_unlocked"); ++i) game.tick(0.05F);
    for (int i = 0; i < 300 && (game.inCutscene() || game.scene().player().moving()); ++i) {
        game.tick(0.05F);
    }
    assert(game.session().hasFlag("red_box_unlocked") && !game.session().hasItem("brass_key"));
    assert(game.scene().navigation().canStandAt(behindLock));

    const auto path = directory / "puzzle.slot";
    std::string error;
    const SavedGame saved = game.savedGame();
    assert(saved.roomId == "first");
    assert(SaveFiles::save(path, saved, error));

    // A new process: the progress goes in before the room, then the feet go back.
    const SaveLoadResult loaded = SaveFiles::load(path);
    assert(loaded.save);
    SessionState progress;
    assert(progress.restore(loaded.save->session, error));
    Game resumed(room, std::move(progress));
    resumed.setInteractions(RoomPresets::firstRoomInteractions());
    const SDL_FPoint feet = resumed.placePlayerNear(loaded.save->feet);
    assert(near(feet.x, saved.feet.x) && near(feet.y, saved.feet.y));
    assert(resumed.session().hasFlag("found_brass_key"));
    assert(resumed.session().hasFlag("red_box_unlocked"));
    assert(!resumed.session().hasItem("brass_key"));
    assert(!resumed.scene().isObjectSolid(resumed.scene().room().scenery[1]));
    assert(resumed.scene().navigation().canStandAt(behindLock));
    // The open box lets the route go straight through where it used to be.
    std::string ignored;
    assert(resumed.teleportPlayer({760.0F, 573.0F}, ignored));
    resumed.onClick({970.0F, 573.0F});
    for (int i = 0; i < 200 && resumed.scene().player().moving(); ++i) resumed.tick(0.05F);
    assert(near(resumed.scene().player().feet().x, 970.0F));
    assert(resumed.savedGame().session == saved.session);

    // Feet saved on a spot that is blocked now fall back to the nearest walkable point.
    Game locked(room);
    const SDL_FPoint fallback = locked.placePlayerNear(behindLock);
    assert(locked.scene().navigation().canStandAt(fallback));
    assert(!near(fallback.x, behindLock.x) || !near(fallback.y, behindLock.y));
}

// Version 2 adds the facing; version 1 files keep loading (facing down) and cannot carry it.
void testSaveFacingAndOlderVersions(const std::filesystem::path& directory) {
    SavedGame saved{"gallery", {300.0F, 500.0F}, {}};
    saved.facing = Facing::left;
    const std::string text = SaveFiles::serialize(saved);
    assert(text.find("version = 2\nroom = gallery\nfeet = 300, 500\nfacing = left\n")
           != std::string::npos);
    const SaveLoadResult parsed = SaveFiles::parse(text);
    assert(parsed.save && *parsed.save == saved);

    const SaveLoadResult older = SaveFiles::parse("version = 1\nroom = first\nfeet = 10, 20\n"
                                                  "item = chave\n");
    assert(older.save && older.save->facing == Facing::down);
    assert(older.save->session.items == std::vector<std::string>{"chave"});
    assert(!SaveFiles::parse("version = 1\nroom = first\nfeet = 10, 20\nfacing = up\n").save);
    assert(SaveFiles::parse("version = 2\nroom = first\nfeet = 10, 20\n").save); // optional
    assert(SaveFiles::parse("version = 2\nroom = first\nfeet = 1, 2\nfacing = norte\n").error
           == "Linha 4: Chave ou valor invalido: facing");

    // The resumed player looks where it looked; saving it again gives the same bytes.
    Game game(RoomPresets::firstRoom());
    game.onClick({300.0F, 360.0F}); // walk left
    for (int i = 0; i < 200 && game.scene().player().moving(); ++i) game.tick(0.05F);
    assert(game.scene().player().facing() == Facing::left);
    const auto path = directory / "facing.slot";
    std::string error;
    assert(SaveFiles::save(path, game.savedGame(), error));
    const SaveLoadResult loaded = SaveFiles::load(path);
    assert(loaded.save && loaded.save->facing == Facing::left);
    Game resumed(RoomPresets::firstRoom());
    assert(resumed.scene().player().facing() == Facing::down);
    (void)resumed.placePlayerNear(loaded.save->feet, loaded.save->facing);
    assert(resumed.scene().player().facing() == Facing::left);
    assert(resumed.savedGame() == *loaded.save);

    // A save edited by hand in a Windows editor (UTF-8 BOM, CRLF) still loads.
    const auto edited = directory / "bom.slot";
    std::ofstream(edited, std::ios::binary)
            << "\xEF\xBB\xBF" << "version = 2\r\nroom = gallery\r\nfeet = 300, 500\r\n";
    const SaveLoadResult fromEditor = SaveFiles::load(edited);
    assert(fromEditor.save && fromEditor.save->roomId == "gallery");
}

// Floats are written as the shortest plain decimal that reads back the same, not with
// max_digits10 noise ("53.3333321"), and never follow the process locale.
void testShortestNumbersInSave() {
    assert(TextFormat::number(300.0F) == "300");
    assert(TextFormat::number(0.1F) == "0.1");
    assert(TextFormat::number(-0.25F) == "-0.25");
    assert(TextFormat::number(1150.5F) == "1150.5");
    assert(TextFormat::number(53.333333F) == "53.333332");
    assert(TextFormat::number(100000.0F) == "100000"); // no exponent
    assert(TextFormat::number(0.0F) == "0");

    const float samples[]{53.333333F, 0.1F, 1.0F / 3.0F, 16777217.0F, 1234.5678F, 0.000123F,
                          -719.99994F, std::numeric_limits<float>::max(),
                          std::numeric_limits<float>::min(),
                          -std::numeric_limits<float>::denorm_min()};
    for (const float sample : samples) {
        const std::string text = TextFormat::number(sample);
        assert(text.find('e') == std::string::npos && text.size() < 64);
        float back = 0.0F;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), back);
        assert(result.ec == std::errc{} && result.ptr == text.data() + text.size());
        assert(back == sample);
    }

    SavedGame saved{"gallery", {53.333333F, 0.1F}, {}};
    const std::string text = SaveFiles::serialize(saved);
    assert(text.find("feet = 53.333332, 0.1\n") != std::string::npos);
    const SaveLoadResult parsed = SaveFiles::parse(text);
    assert(parsed.save && *parsed.save == saved);

    // A decimal-comma locale changes nothing in the file.
    const std::locale previous;
    const std::string previousC = std::setlocale(LC_ALL, nullptr);
    for (const char* name : {"pt_BR.UTF-8", "de_DE.UTF-8"}) {
        try {
            std::locale::global(std::locale(name));
        } catch (const std::runtime_error&) {
            continue; // not installed here
        }
        assert(SaveFiles::serialize(saved) == text);
        const SaveLoadResult localized = SaveFiles::parse(text);
        assert(localized.save && *localized.save == saved);
    }
    std::locale::global(previous);
    std::setlocale(LC_ALL, previousC.c_str());
}

void testBadSaveFiles(const std::filesystem::path& directory) {
    const auto errorOf = [](const std::string_view text) {
        const SaveLoadResult result = SaveFiles::parse(text);
        assert(!result.save);
        return result.error;
    };
    const std::string header = "version = 1\nroom = first\nfeet = 10, 20\n";
    assert(SaveFiles::parse(header + "# ok\nflag.a = -2\n").save);
    assert(errorOf("") == "Save vazio ou sem version");
    assert(errorOf("# so comentario\n") == "Save vazio ou sem version");
    assert(errorOf("version = 3\nroom = first\n") == "Linha 1: Versao de save desconhecida: 3");
    assert(errorOf("version = 0\n") == "Linha 1: Versao de save desconhecida: 0");
    assert(errorOf("room = first\nversion = 1\n") == "Linha 1: A primeira chave deve ser version");
    assert(errorOf(header + "camera = 1, 2\n") == "Linha 4: Chave desconhecida: camera");
    assert(errorOf(header + "item = chave!\n") == "Linha 4: Chave ou valor invalido: item");
    assert(errorOf(header + "item = chave\nitem = chave\n") == "Linha 5: Item repetido: chave");
    assert(errorOf(header + "flag.a = 1\nflag.a = 2\n") == "Linha 5: Chave repetida: flag.a");
    assert(errorOf(header + "flag.a = 0\n") == "Linha 4: Chave ou valor invalido: flag.a");
    assert(errorOf(header + "flag.a b = 1\n") == "Linha 4: Chave ou valor invalido: flag.a b");
    assert(errorOf(header + "object.first.box.visible = 2\n")
           == "Linha 4: Chave ou valor invalido: object.first.box.visible");
    assert(errorOf(header + "object.first.box.color = 1\n")
           == "Linha 4: Chave ou valor invalido: object.first.box.color");
    assert(errorOf(header + "dialogue = a/b\n") == "Linha 4: Chave ou valor invalido: dialogue");
    assert(errorOf(header + "item chave\n") == "Linha 4: Esperado chave = valor");
    assert(errorOf("version = 1\nroom = sala x\n") == "Linha 2: Chave ou valor invalido: room");
    assert(errorOf("version = 1\nroom = first\nfeet = nan, 1\n")
           == "Linha 3: Chave ou valor invalido: feet");
    assert(errorOf("version = 1\nfeet = 1, 2\n") == "Falta a chave room");
    assert(errorOf("version = 1\nroom = first\n") == "Falta a chave feet");

    // An invalid snapshot never touches the progress in place.
    SessionState session;
    session.addItem("chave");
    SessionSnapshot invalid;
    invalid.items = {"ok", "nao ok"};
    std::string error;
    assert(!session.restore(invalid, error) && error == "Item invalido: nao ok");
    assert(session.hasItem("chave") && !session.hasItem("ok"));

    // Boot: missing = new game quietly; refused = new game, the file kept as .bad; a room that
    // no longer loads keeps the progress in the start room.
    const auto rooms = directory / "save-rooms";
    std::filesystem::create_directories(rooms);
    assert(RoomFiles::save(rooms / "first.room", RoomPresets::firstRoom(), error));
    RoomLibrary library;
    assert(library.open(rooms, error));
    const auto slot = directory / "boot.slot";
    ResumeResult none = SaveFiles::resume(slot, library);
    assert(!none.save && none.error.empty());

    std::ofstream(slot) << "version = 2\nroom = sumida\nfeet = 10, 20\nfacing = up\n"
                           "flag.porta = 1\nitem = chave\nobject.sumida.cofre.visible = 0\n";
    ResumeResult moved = SaveFiles::resume(slot, library);
    assert(moved.save && moved.bundle && moved.error.empty());
    assert(moved.warning.starts_with("Sala sumida: "));
    assert(moved.bundle->room.id == "first" && moved.save->roomId == "first");
    const Room start = RoomPresets::firstRoom();
    assert(moved.save->feet.x == start.playerStart.x && moved.save->feet.y == start.playerStart.y);
    assert(moved.save->facing == Facing::down);
    assert(moved.save->session.items == std::vector<std::string>{"chave"});
    assert(moved.save->session.flags.size() == 1 && moved.save->session.objects.size() == 1);
    assert(std::filesystem::exists(slot) && !std::filesystem::exists(directory / "boot.slot.bad"));

    // Refused only when the start room does not load either.
    std::ofstream(slot) << "version = 1\nroom = sumida\nfeet = 10, 20\n";
    ResumeResult gone = SaveFiles::resume(slot, library, "inexistente.room");
    assert(!gone.save && !gone.bundle && gone.warning.empty());
    assert(gone.error.starts_with("Sala sumida: ") && gone.error.find("; inexistente.room: ")
                                                      != std::string::npos);
    assert(!std::filesystem::exists(slot));
    assert(readAll(directory / "boot.slot.bad").find("room = sumida") != std::string::npos);

    std::ofstream(slot) << "version = 9\n";
    ResumeResult future = SaveFiles::resume(slot, library);
    assert(!future.save && future.error == "Linha 1: Versao de save desconhecida: 9");
    assert(readAll(directory / "boot.slot.bad") == "version = 9\n"); // the newest refused one

    std::ofstream(slot) << "version = 1\nroom = first\nfeet = 300, 600\nitem = chave\n";
    ResumeResult ok = SaveFiles::resume(slot, library);
    assert(ok.save && ok.bundle && ok.error.empty());
    assert(ok.bundle->room.id == "first" && ok.save->session.items.size() == 1);
    assert(std::filesystem::exists(slot));
}

void testAtomicSaveFailureKeepsPrevious(const std::filesystem::path& directory) {
    const auto path = directory / "atomic.slot";
    std::string error;
    SavedGame saved{"first", {100.0F, 600.0F}, {}};
    saved.session.items = {"chave"};
    assert(SaveFiles::save(path, saved, error));
    const std::string previous = readAll(path);

    // Missing folder: the write fails and creates nothing.
    assert(!SaveFiles::save(directory / "nao-existe" / "atomic.slot", saved, error));
    assert(!error.empty());
    assert(!std::filesystem::exists(directory / "nao-existe"));

    // The temporary cannot be written (a folder sits on its name): the old save survives.
    std::filesystem::create_directory(directory / "atomic.slot.tmp");
    saved.session.items = {"outra"};
    error.clear();
    assert(!SaveFiles::save(path, saved, error) && !error.empty());
    assert(readAll(path) == previous);
    std::filesystem::remove(directory / "atomic.slot.tmp");

    // Invalid data is refused before any write.
    saved.session.items = {"nao ok"};
    assert(!SaveFiles::save(path, saved, error) && error == "Item invalido: nao ok");
    assert(readAll(path) == previous);
}

void testAutosavePolicy() {
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
    assert(first && first->roomId == "first");
    autosave.markWritten();
    assert(!autosave.enterBackground(game));
    assert(autosave.inBackground());
    // Back and away again with nothing new: the file already has it.
    autosave.enterForeground();
    assert(!autosave.enterBackground(game));
    assert(!autosave.quit(game));

    // A failed write is retried on the next trip, still once per trip.
    autosave.enterForeground();
    game.changeSession([](SessionState& session) { session.setFlag("progresso"); });
    assert(autosave.enterBackground(game));
    assert(!autosave.enterBackground(game)); // DID after WILL, the write failed
    autosave.enterForeground();
    assert(autosave.enterBackground(game));
    autosave.markWritten();
    autosave.enterForeground();

    // Mid-cutscene the snapshot stays the last stable one.
    game.onClick({430.0F, 410.0F});
    for (int i = 0; i < 300 && !game.inCutscene(); ++i) game.tick(0.05F);
    assert(game.inCutscene());
    game.changeSession([](SessionState& session) { session.setFlag("meio_da_cena"); });
    autosave.capture(game);
    assert(!autosave.enterBackground(game)); // nothing stable that is not on disk yet
    const auto& stable = autosave.stable()->session.flags;
    assert(std::none_of(stable.begin(), stable.end(),
                        [](const auto& flag) { return flag.id == "meio_da_cena"; }));
    autosave.enterForeground();

    // Once the sequence ends, quitting writes the finished state.
    game.onClick({200.0F, 600.0F}); // skip the line
    for (int i = 0; i < 20 && game.inCutscene(); ++i) game.tick(0.05F);
    assert(!game.inCutscene() && game.session().hasFlag("looked"));
    const SavedGame* atQuit = autosave.quit(game);
    assert(atQuit && atQuit->session == game.session().snapshot());
    autosave.markWritten();
    assert(!autosave.quit(game)); // SDL_EVENT_QUIT then TERMINATING

    // Entering a room is a checkpoint: written when it differs from the file, so a crash costs
    // at most the current room; never the half-done state of a cutscene.
    assert(!autosave.checkpoint(game)); // the file already has this
    game.changeSession([](SessionState& session) { session.setFlag("outra_sala"); });
    const SavedGame* entered = autosave.checkpoint(game);
    assert(entered && entered->session == game.session().snapshot());
    assert(autosave.checkpoint(game)); // the write failed: still pending
    autosave.markWritten();
    assert(!autosave.checkpoint(game));
    game.onClick({430.0F, 410.0F});
    for (int i = 0; i < 300 && !game.inCutscene(); ++i) game.tick(0.05F);
    assert(game.inCutscene());
    game.changeSession([](SessionState& session) { session.setFlag("no_meio"); });
    assert(!autosave.checkpoint(game));
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
    testSessionCommandKeepsNavigation();
    testConditionalNpcPuzzleAndRoomPersistence();
    testScriptedSequenceAndCutscene();
    testDialogueWithChoices();
    testGameEventQueue();
    testRoomSpawns();
    testNearestReachableInLargeRoom();
    testCameraTargetFor();

    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("pointclick-save-tests-" + std::to_string(suffix));
    std::filesystem::create_directory(directory);
    testSaveRoundTrip(directory);
    testSaveRestoresPuzzleProgress(directory);
    testSaveFacingAndOlderVersions(directory);
    testShortestNumbersInSave();
    testBadSaveFiles(directory);
    testAtomicSaveFailureKeepsPrevious(directory);
    testAutosavePolicy();
    std::filesystem::remove_all(directory);
}
