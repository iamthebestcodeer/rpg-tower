// Tests for waves.c (ManageWaves) and update.c (state machine, input, env).
#include "test_util.h"

// No enemy active -> sentinel -1 (int-backed enum, so valid; mirrors the
// TOWER_NONE = -1 convention in game.h) rather than a real type like ENEMY_BASIC.
static EnemyType FirstActiveEnemyType(void) {
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (game.enemies[i].active) return game.enemies[i].type;
    return (EnemyType)-1;
}

static void TestWaveStart(void) {
    ResetForTest();
    game.state = GS_PLAYING;
    game.waveTimer = 0.5f;
    game.waveActive = false;
    ManageWaves(0.6f);
    CHECK(game.waveActive);
    CHECK(game.currentWave == 1);
    CHECK(game.enemiesToSpawn == 13); // 10 + (int)(1 * 3.5)
    CHECK(game.spawnTimer == 1.0f);

    // N key force-starts
    ResetForTest();
    game.state = GS_PLAYING;
    game.waveTimer = 50.0f;
    StubPressKey(KEY_N);
    ManageWaves(0.1f);
    CHECK(game.currentWave == 1);
    CHECK(game.waveActive);
}

static void TestWaveSpawnTypes(void) {
    // wave 1 -> basic
    ResetForTest();
    game.currentWave = 1; game.waveActive = true; game.enemiesToSpawn = 3; game.spawnTimer = 0.01f;
    StubSetRandomValue(50);
    ManageWaves(0.1f);
    CHECK(CountActiveEnemies() == 1);
    CHECK(FirstActiveEnemyType() == ENEMY_BASIC);
    CHECK(game.enemiesToSpawn == 2);

    // wave 2, rand < 30 -> fast
    ResetForTest();
    game.currentWave = 2; game.waveActive = true; game.enemiesToSpawn = 3; game.spawnTimer = 0.01f;
    StubSetRandomValue(0);
    ManageWaves(0.1f);
    CHECK(FirstActiveEnemyType() == ENEMY_FAST);

    // wave 4, rand 75 -> tank
    ResetForTest();
    game.currentWave = 4; game.waveActive = true; game.enemiesToSpawn = 3; game.spawnTimer = 0.01f;
    StubSetRandomValue(75);
    ManageWaves(0.1f);
    CHECK(FirstActiveEnemyType() == ENEMY_TANK);

    // wave 6, rand 55 -> ethereal
    ResetForTest();
    game.currentWave = 6; game.waveActive = true; game.enemiesToSpawn = 3; game.spawnTimer = 0.01f;
    StubSetRandomValue(55);
    ManageWaves(0.1f);
    CHECK(FirstActiveEnemyType() == ENEMY_ETHEREAL);

    // wave 8, rand 90 -> healer
    ResetForTest();
    game.currentWave = 8; game.waveActive = true; game.enemiesToSpawn = 3; game.spawnTimer = 0.01f;
    StubSetRandomValue(90);
    ManageWaves(0.1f);
    CHECK(FirstActiveEnemyType() == ENEMY_HEALER);

    // wave 10, rand 45 -> spawner
    ResetForTest();
    game.currentWave = 10; game.waveActive = true; game.enemiesToSpawn = 3; game.spawnTimer = 0.01f;
    StubSetRandomValue(45);
    ManageWaves(0.1f);
    CHECK(FirstActiveEnemyType() == ENEMY_SPAWNER);
}

static void TestWaveBoss(void) {
    ResetForTest();
    game.currentWave = 10; game.waveActive = true; game.enemiesToSpawn = 1; game.spawnTimer = 0.01f;
    StubSetRandomValue(20);
    ManageWaves(0.1f);
    CHECK(FirstActiveEnemyType() == ENEMY_BOSS);
    CHECK(game.enemiesToSpawn == 0);
}

static void TestWaveSpawnerBonus(void) {
    ResetForTest();
    game.currentWave = 5; game.waveActive = true; game.enemiesToSpawn = 10; game.spawnTimer = 0.01f;
    StubSetRandomValue(50); // basic for the regular spawn
    ManageWaves(0.1f);
    // bonus spawner + regular spawn
    CHECK(CountActiveEnemies() == 2);
    CHECK(game.enemiesToSpawn == 8);
    int spawners = 0;
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (game.enemies[i].active && game.enemies[i].type == ENEMY_SPAWNER) spawners++;
    CHECK(spawners == 1);
}

static void TestWaveComplete(void) {
    ResetForTest();
    game.currentWave = 3;
    game.waveActive = true;
    game.enemiesToSpawn = 0;
    int gold0 = game.gold, aether0 = game.aether;
    ManageWaves(0.1f);
    CHECK(!game.waveActive);
    CHECK(game.waveTimer == WAVE_INTERVAL);
    CHECK(game.gold == gold0 + 50 + 3 * 20);
    CHECK(game.aether == aether0 + 5 + 3 / 3);
}

static void TestUpdateGameStates(void) {
    // TITLE -> ENTER -> PLAYING
    ResetForTest();
    game.state = GS_TITLE;
    StubPressKey(KEY_ENTER);
    UpdateGame(0.1f);
    CHECK(game.state == GS_PLAYING);
    CHECK(game.gold == STARTING_GOLD); // ResetGame ran

    // TITLE without input stays; env skipped
    ResetForTest();
    game.state = GS_TITLE;
    game.dayNightCycle = 0.5f;
    game.globalTime = 0.5f;
    UpdateGame(0.1f);
    CHECK(game.state == GS_TITLE);
    CHECK_NEAR(game.dayNightCycle, 0.5f, 0.0001f);

    // TITLE -> mouse click also starts
    ResetForTest();
    game.state = GS_TITLE;
    StubClickMouse(MOUSE_LEFT_BUTTON);
    UpdateGame(0.1f);
    CHECK(game.state == GS_PLAYING);

    // PLAYING -> P -> PAUSED; PAUSED -> P -> PLAYING
    ResetForTest();
    game.state = GS_PLAYING;
    game.waveTimer = 999.0f;
    StubPressKey(KEY_P);
    UpdateGame(0.1f);
    CHECK(game.state == GS_PAUSED);
    StubPressKey(KEY_P);
    UpdateGame(0.1f);
    CHECK(game.state == GS_PLAYING);

    // PAUSED + ESCAPE -> PLAYING
    game.state = GS_PAUSED;
    StubPressKey(KEY_ESCAPE);
    UpdateGame(0.1f);
    CHECK(game.state == GS_PLAYING);

    // PLAYING with skill point -> LEVEL_UP
    ResetForTest();
    game.state = GS_PLAYING;
    game.hero.skillPoints = 1;
    game.waveTimer = 999.0f;
    UpdateGame(0.1f);
    CHECK(game.state == GS_LEVEL_UP_HERO);

    // LEVEL_UP with no points -> PLAYING
    game.hero.skillPoints = 0;
    UpdateGame(0.1f);
    CHECK(game.state == GS_PLAYING);

    // GAME_OVER -> R -> PLAYING
    ResetForTest();
    game.state = GS_GAME_OVER;
    StubPressKey(KEY_R);
    UpdateGame(0.1f);
    CHECK(game.state == GS_PLAYING);
}

static void TestPlayingGameOver(void) {
    ResetForTest();
    game.state = GS_PLAYING;
    game.waveActive = false;
    game.waveTimer = 999.0f;
    game.lives = 1;
    int e = SpawnEnemyAt(ENEMY_BASIC, -60, 140);
    game.enemies[e].waypointIndex = game.map.waypointCount; // leaks now
    UpdatePlaying(0.1f);
    CHECK(game.lives == 0);
    CHECK(game.state == GS_GAME_OVER);
    CHECK(game.screenShakeTime > 0);
}

static void TestUpdateEnvironment(void) {
    ResetForTest();
    game.dayNightCycle = 1.99f;
    UpdateEnvironment(60.0f); // +0.5 -> wraps to 0.49
    CHECK_NEAR(game.dayNightCycle, 0.49f, 0.001f);
    CHECK(game.environmentColor.r > 0);

    // screen shake drives camera via RNG
    ResetForTest();
    game.screenShakeIntensity = 5.0f;
    game.screenShakeTime = 1.0f;
    game.screenShakeDuration = 1.0f;
    StubSetRandomValue(100); // offset clamped to +/- 4
    UpdateEnvironment(0.1f);
    CHECK(game.screenShakeTime < 1.0f);
    CHECK(game.camera.offset.x != 0.0f || game.camera.offset.y != 0.0f);

    // after shake, offsets ease back to zero (raymath Lerp doesn't clamp the
    // amount, so keep dt small enough that 10*dt <= 1)
    game.screenShakeTime = 0.0f;
    game.camera.offset.x = 4.0f;
    game.camera.offset.y = 4.0f;
    UpdateEnvironment(0.1f);
    CHECK_NEAR(game.camera.offset.x, 0.0f, 0.01f);
    CHECK_NEAR(game.camera.offset.y, 0.0f, 0.01f);
}

static void TestHandleInput(void) {
    ResetForTest();
    game.state = GS_PLAYING;
    StubSetMousePosition(100, 100); // world (100,100) -> tile (2,2)
    StubClickMouse(MOUSE_LEFT_BUTTON);
    game.placingTower = TOWER_PULSE;
    HandleInput();
    CHECK(CountActiveTowers() == 1);
    CHECK(game.gold == 250);
    CHECK(game.placingTower == TOWER_NONE); // no shift -> cleared

    // shift keeps placing mode
    ResetForTest();
    game.state = GS_PLAYING;
    StubSetMousePosition(100, 100);
    StubClickMouse(MOUSE_LEFT_BUTTON);
    StubSetKeyDown(KEY_LEFT_SHIFT, true);
    game.placingTower = TOWER_CANNON;
    HandleInput();
    CHECK(CountActiveTowers() == 1);
    CHECK(game.placingTower == TOWER_CANNON);

    // click selects an existing tower
    ResetForTest();
    game.state = GS_PLAYING;
    PlaceTower(2, 2, TOWER_PULSE);
    StubSetMousePosition(100, 100);
    StubClickMouse(MOUSE_LEFT_BUTTON);
    HandleInput();
    CHECK(game.selectedTowerIndex == 0);

    // right-click cancels
    ResetForTest();
    game.state = GS_PLAYING;
    game.placingTower = TOWER_CRYO;
    game.selectedTowerIndex = 3;
    StubClickMouse(MOUSE_RIGHT_BUTTON);
    HandleInput();
    CHECK(game.placingTower == TOWER_NONE);
    CHECK(game.selectedTowerIndex == -1);

    // number keys select build types
    ResetForTest();
    game.state = GS_PLAYING;
    StubPressKey(KEY_ONE);
    HandleInput();
    CHECK(game.placingTower == TOWER_PULSE);
    StubPressKey(KEY_TWO);
    HandleInput();
    CHECK(game.placingTower == TOWER_CANNON);
    StubPressKey(KEY_THREE);
    HandleInput();
    CHECK(game.placingTower == TOWER_CRYO);
    StubPressKey(KEY_FOUR);
    HandleInput();
    CHECK(game.placingTower == TOWER_TESLA);

    // escape cancels
    StubPressKey(KEY_ESCAPE);
    HandleInput();
    CHECK(game.placingTower == TOWER_NONE);
}

static void TestMouseClickFrameScope(void) {
    // A click set by StubClickMouse must only register in the frame it was set
    // in: after a DrawGame() frame boundary, IsMouseButtonPressed must return
    // false again (raylib scopes pressed edges to a single frame). Without
    // that, the second HandleInput would see a phantom repeat click and select
    // the tower it just placed.
    ResetForTest();
    game.state = GS_PLAYING;
    StubSetMousePosition(100, 100); // world (100,100) -> tile (2,2)
    StubClickMouse(MOUSE_LEFT_BUTTON);
    game.placingTower = TOWER_PULSE;
    HandleInput();
    CHECK(CountActiveTowers() == 1); // frame 1 consumes the click

    DrawGame(); // frame boundary: pressed edges expire

    // frame 2: same click must not place another tower or select one
    int towers = CountActiveTowers();
    HandleInput();
    CHECK(CountActiveTowers() == towers);
    CHECK(game.selectedTowerIndex == -1);
}

static void TestUpdateHeroLevelUpDirect(void) {
    ResetForTest();
    game.state = GS_LEVEL_UP_HERO;
    game.hero.skillPoints = 0;
    UpdateHeroLevelUp(0.1f);
    CHECK(game.state == GS_PLAYING);
}

void TestWavesUpdate(void) {
    TestWaveStart();
    TestWaveSpawnTypes();
    TestWaveBoss();
    TestWaveSpawnerBonus();
    TestWaveComplete();
    TestUpdateGameStates();
    TestPlayingGameOver();
    TestUpdateEnvironment();
    TestHandleInput();
    TestMouseClickFrameScope();
    TestUpdateHeroLevelUpDirect();
}
