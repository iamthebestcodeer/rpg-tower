// Tests for utils.c (geometry, metadata, batched circles) and main.c
// (init/reset/map).
#include "test_util.h"

static void TestWorldTile(void) {
    Vector2 v = WorldToTile((Vector2){40, 40});
    CHECK(v.x == 1 && v.y == 1);
    v = WorldToTile((Vector2){-1, 3});
    CHECK(v.x == -1 && v.y == 0);
    v = WorldToTile((Vector2){0, 0});
    CHECK(v.x == 0 && v.y == 0);
    v = WorldToTile((Vector2){79, 79});
    CHECK(v.x == 1 && v.y == 1);
    v = WorldToTile((Vector2){80, 80});
    CHECK(v.x == 2 && v.y == 2);
}

static void TestTileToWorld(void) {
    Vector2 v = TileToWorldCenter(2, 3);
    CHECK(v.x == 100 && v.y == 140);
    v = TileToWorldCenter(0, 0);
    CHECK(v.x == 20 && v.y == 20);
}

static void TestClosestPointOnSegment(void) {
    Vector2 a = {0, 0}, b = {100, 0};
    Vector2 c = ClosestPointOnSegment((Vector2){50, 50}, a, b);
    CHECK(c.x == 50 && c.y == 0);
    c = ClosestPointOnSegment((Vector2){-50, 10}, a, b);
    CHECK(c.x == 0 && c.y == 0);
    c = ClosestPointOnSegment((Vector2){150, -10}, a, b);
    CHECK(c.x == 100 && c.y == 0);
    c = ClosestPointOnSegment((Vector2){10, 10}, a, a); // zero-length
    CHECK(c.x == 0 && c.y == 0);
    c = ClosestPointOnSegment((Vector2){60, 0}, a, b); // on segment (float math)
    CHECK_NEAR(c.x, 60.0f, 0.001);
    CHECK_NEAR(c.y, 0.0f, 0.001);
}

static void TestIsTileBuildable(void) {
    ResetForTest();
    CHECK(IsTileBuildable(5, 5));
    CHECK(!IsTileBuildable(-1, 0));
    CHECK(!IsTileBuildable(0, -1));
    CHECK(!IsTileBuildable(MAP_WIDTH, 0));
    CHECK(!IsTileBuildable(0, MAP_HEIGHT));
    CHECK(!IsTileBuildable(0, 3)); // path tile
    game.occupied[5][5] = true;
    CHECK(!IsTileBuildable(5, 5));
}

static void TestEnemyLookup(void) {
    ResetForTest();
    CHECK(GetEnemyById(1) == NULL);
    CHECK(GetEnemyIndexById(1) == -1);
    CHECK(GetEnemyById(0) == NULL);
    int idx = SpawnEnemyAt(ENEMY_BASIC, 100, 100);
    int id = game.enemies[idx].id;
    CHECK(GetEnemyById(id) == &game.enemies[idx]);
    CHECK(GetEnemyIndexById(id) == idx);
    game.enemies[idx].active = false;
    CHECK(GetEnemyById(id) == NULL);
    CHECK(GetEnemyIndexById(id) == -1);
}

static void TestAngles(void) {
    CHECK_NEAR(LerpAngle(0, 90, 0.5f), 45, 0.001);
    CHECK_NEAR(LerpAngle(350, 10, 0.5f), 360.0f, 0.001); // wraps the short way
    CHECK_NEAR(LerpAngle(10, 350, 0.5f), 0.0f, 0.001);
    CHECK_NEAR(LerpAngle(100, 100, 0.5f), 100.0f, 0.001);
    CHECK_NEAR(LerpAngle(0, 360, 0.0f), 0.0f, 0.001);
    CHECK_NEAR(GetAngleDifference(10, 350), 20, 0.001);
    CHECK_NEAR(GetAngleDifference(350, 10), 20, 0.001);
    CHECK_NEAR(GetAngleDifference(0, 180), 180, 0.001);
    CHECK_NEAR(GetAngleDifference(45, 45), 0, 0.001);
}

static void TestTowerMeta(void) {
    CHECK(GetTowerCost(TOWER_PULSE) == 100);
    CHECK(GetTowerCost(TOWER_CANNON) == 250);
    CHECK(GetTowerCost(TOWER_CRYO) == 200);
    CHECK(GetTowerCost(TOWER_TESLA) == 400);
    CHECK(GetTowerCost(TOWER_T4_PULSE_REPEATER) == 800);
    CHECK(GetTowerCost(TOWER_T4_PULSE_SNIPER) == 1000);
    CHECK(GetTowerCost(TOWER_T4_CANNON_MORTAR) == 1500);
    CHECK(GetTowerCost(TOWER_T4_CANNON_VULCAN) == 900);
    CHECK(GetTowerCost(TOWER_T4_CRYO_BLIZZARD) == 700);
    CHECK(GetTowerCost(TOWER_T4_CRYO_FREEZER) == 850);
    CHECK(GetTowerCost(TOWER_T4_TESLA_CHAIN) == 1100);
    CHECK(GetTowerCost(TOWER_T4_TESLA_STORM) == 1300);
    CHECK(GetTowerCost(TOWER_NONE) == 99999);

    CHECK(GetTowerAetherCost(TOWER_PULSE) == 0);
    CHECK(GetTowerAetherCost(TOWER_T4_PULSE_REPEATER) == 50 + 800 / 20);
    CHECK(GetTowerAetherCost(TOWER_T4_CANNON_MORTAR) == 50 + 1500 / 20);

    CHECK_STREQ(GetTowerName(TOWER_PULSE), "Pulse Emitter [1]");
    CHECK_STREQ(GetTowerName(TOWER_NONE), "Unknown");
    for (int t = TOWER_PULSE; t <= TOWER_T4_TESLA_STORM; t++) {
        const char* n = GetTowerName((TowerType)t);
        const char* d = GetTowerDescription((TowerType)t);
        CHECK(n != NULL && n[0] != '\0');
        CHECK(d != NULL && d[0] != '\0');
    }
    CHECK_STREQ(GetTowerDescription(TOWER_NONE), "No description available.");

    Color c = GetTowerColor(TOWER_PULSE);
    CHECK(c.r == COLOR_ENERGY.r && c.g == COLOR_ENERGY.g && c.b == COLOR_ENERGY.b);
    c = GetTowerColor(TOWER_CANNON);
    CHECK(c.r == COLOR_PHYSICAL.r && c.b == COLOR_PHYSICAL.b);
    c = GetTowerColor(TOWER_CRYO);
    CHECK(c.r == COLOR_CRYO.r && c.b == COLOR_CRYO.b);
    c = GetTowerColor(TOWER_TESLA);
    CHECK(c.r == COLOR_TESLA.r && c.b == COLOR_TESLA.b);
    c = GetTowerColor(TOWER_NONE);
    CHECK(c.r == GRAY.r && c.g == GRAY.g);

    CHECK_NEAR(GetAimToleranceDegrees(TOWER_PULSE), 30.0f, 0.001);
    CHECK_NEAR(GetAimToleranceDegrees(TOWER_T4_PULSE_REPEATER), 30.0f, 0.001);
    CHECK_NEAR(GetAimToleranceDegrees(TOWER_T4_CANNON_VULCAN), 30.0f, 0.001);
    CHECK_NEAR(GetAimToleranceDegrees(TOWER_T4_PULSE_SNIPER), 12.0f, 0.001);
    CHECK_NEAR(GetAimToleranceDegrees(TOWER_CANNON), 12.0f, 0.001);
    CHECK_NEAR(GetAimToleranceDegrees(TOWER_T4_CANNON_MORTAR), 12.0f, 0.001);
    CHECK_NEAR(GetAimToleranceDegrees(TOWER_CRYO), 15.0f, 0.001);
    CHECK_NEAR(GetAimToleranceDegrees(TOWER_T4_CRYO_FREEZER), 15.0f, 0.001);
    CHECK_NEAR(GetAimToleranceDegrees(TOWER_TESLA), 20.0f, 0.001);
    CHECK_NEAR(GetAimToleranceDegrees(TOWER_T4_TESLA_STORM), 20.0f, 0.001);
}

static void TestEmitCircleFan(void) {
    ResetForTest();

    // Non-positive radii short-circuit: no geometry is submitted. Note that
    // EmitCircleFan only emits vertices; the caller owns the rlBegin block.
    StubResetRlglLog();
    EmitCircleFan((Vector2){10, 10}, 0.0f, WHITE);   // early return
    EmitCircleFan((Vector2){10, 10}, -1.0f, WHITE);  // early return
    CHECK(StubRlBeginCount() == 0);
    CHECK(StubRlVertexCount() == 0);

    // Positive radii emit an adaptive fan (3 vertices per segment): segment
    // count grows with radius - 8 for a 5px circle, 4 for a 2px one.
    StubResetRlglLog();
    EmitCircleFan((Vector2){10, 10}, 5.0f, WHITE);   // 8 segments
    EmitCircleFan((Vector2){20, 20}, 2.0f, RED);     // cached unit circle, 4 segments
    CHECK(StubRlBeginCount() == 0);
    CHECK(StubRlVertexCount() == (8 + 4) * 3);
}

static void TestInitResetMap(void) {
    // InitGame already ran in the runner; verify invariants
    CHECK(game.state == GS_TITLE);
    CHECK(game.lives == STARTING_LIVES);
    CHECK(game.gold == STARTING_GOLD);
    CHECK(game.aether == STARTING_AETHER);
    CHECK(game.hero.level == 1);
    CHECK(game.hero.xpToNextLevel == 250);
    CHECK(game.hero.skillPoints == 0);
    CHECK(game.hero.attackDamage == 35);
    CHECK(game.mapRTBuilt);
    CHECK(game.map.waypointCount > 10);
    CHECK(game.map.tiles[3][0] == 1); // path
    CHECK(game.map.tiles[5][5] == 0); // grass
    CHECK(game.map.waypoints[0].x == -60 && game.map.waypoints[0].y == 140);

    // ResetGame restores baseline
    game.gold = 0; game.lives = 0; game.currentWave = 7; game.placingTower = TOWER_TESLA;
    ResetGame();
    CHECK(game.gold == STARTING_GOLD);
    CHECK(game.lives == STARTING_LIVES);
    CHECK(game.currentWave == 0);
    CHECK(game.waveTimer == 10.0f);
    CHECK(game.waveActive == false);
    CHECK(game.placingTower == TOWER_NONE);
    CHECK(game.selectedTowerIndex == -1);
    CHECK(game.enemyIdCounter == 1);
    CHECK(CountActiveTowers() == 0);
    CHECK(CountActiveEnemies() == 0);
    CHECK(CountActiveProjectiles() == 0);
    CHECK(game.hero.position.x == 100 && game.hero.position.y == 100);

    // InitMap is idempotent
    InitMap();
    CHECK(game.map.waypointCount > 10);
    CHECK(game.map.tiles[3][0] == 1);
    CHECK(game.map.waypoints[0].x == -60);

    // BuildStaticMapRT: no-op when already built; rebuilds when cleared
    bool wasBuilt = game.mapRTBuilt;
    BuildStaticMapRT();
    CHECK(game.mapRTBuilt == wasBuilt);
    game.mapRTBuilt = false;
    BuildStaticMapRT();
    CHECK(game.mapRTBuilt);
}

void TestUtilsAndMain(void) {
    TestWorldTile();
    TestTileToWorld();
    TestClosestPointOnSegment();
    TestIsTileBuildable();
    TestEnemyLookup();
    TestAngles();
    TestTowerMeta();
    TestEmitCircleFan();
    TestInitResetMap();
}
