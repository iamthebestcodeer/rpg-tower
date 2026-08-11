// Tests for enemies.c: grid, radius queries, spawn, movement, statuses, death.
#include "test_util.h"

static void TestGrid(void) {
    ResetForTest();
    // exact placement: SpawnEnemy would snap these onto the path waypoints
    PlaceEnemyExact(0, ENEMY_BASIC, 100, 100, 100); // tile (2,2)
    PlaceEnemyExact(1, ENEMY_BASIC, 60, 100, 100);  // tile (1,2)
    RebuildEnemyGrid();
    GridCell* cell = &game.enemyGrid[2][2];
    bool foundA = false;
    for (int k = 0; k < cell->count; k++) if (cell->indices[k] == 0) foundA = true;
    CHECK(foundA);

    game.enemies[1].active = false;
    RebuildEnemyGrid();
    cell = &game.enemyGrid[2][2];
    foundA = false;
    for (int k = 0; k < cell->count; k++) if (cell->indices[k] == 0) foundA = true;
    CHECK(foundA);
    // b's cell should no longer contain b
    cell = &game.enemyGrid[2][1];
    for (int k = 0; k < cell->count; k++) CHECK(cell->indices[k] != 1);
}

static void TestGetEnemiesInRadius(void) {
    ResetForTest();
    int idxs[MAX_ENEMIES];
    int count = 0;
    GetEnemiesInRadius((Vector2){200, 200}, 100.0f, idxs, &count, MAX_ENEMIES);
    CHECK(count == 0);

    PlaceEnemyExact(0, ENEMY_BASIC, 200, 220, 100); // dist 20
    PlaceEnemyExact(1, ENEMY_BASIC, 350, 200, 100); // dist 150 -> outside
    RebuildEnemyGrid();
    count = 0;
    GetEnemiesInRadius((Vector2){200, 200}, 100.0f, idxs, &count, MAX_ENEMIES);
    CHECK(count == 1);
    CHECK(idxs[0] == 0);

    // maxOut cap
    PlaceEnemyExact(2, ENEMY_BASIC, 200, 230, 100);
    PlaceEnemyExact(3, ENEMY_BASIC, 200, 240, 100);
    RebuildEnemyGrid();
    count = 0;
    GetEnemiesInRadius((Vector2){200, 200}, 100.0f, idxs, &count, 2);
    CHECK(count == 2);

    // center outside the grid bounds
    count = 0;
    GetEnemiesInRadius((Vector2){-500, -500}, 50.0f, idxs, &count, MAX_ENEMIES);
    CHECK(count == 0);
}

static void TestSpawnEnemyTypes(void) {
    ResetForTest();
    game.currentWave = 1; // hp scaling at a non-zero wave
    int first = SpawnEnemyAt(ENEMY_BASIC, -60, 140); // exactly at waypoint 0
    Enemy* e = &game.enemies[first];
    CHECK(e->active);
    CHECK(e->type == ENEMY_BASIC);
    CHECK(e->hp > 0);
    CHECK(e->hp == e->maxHp);
    CHECK(e->speed == e->baseSpeed);
    CHECK(e->waypointIndex == 1); // at waypoint 0 -> next
    int id1 = e->id;

    int idx = SpawnEnemyAt(ENEMY_TANK, -60, 140);
    CHECK(game.enemies[idx].maxHp > 500);
    CHECK(game.enemies[idx].goldValue == 30);

    idx = SpawnEnemyAt(ENEMY_BOSS, -60, 140);
    CHECK(game.enemies[idx].maxHp > 4000);
    CHECK(game.enemies[idx].aetherValue == 50);

    idx = SpawnEnemyAt(ENEMY_SPAWNER, -60, 140);
    CHECK(game.enemies[idx].aetherValue == 10);

    idx = SpawnEnemyAt(ENEMY_MINION, -60, 140);
    CHECK(game.enemies[idx].xpValue == 5);
    CHECK(game.enemies[idx].goldValue == 3);

    idx = SpawnEnemyAt(ENEMY_FAST, -60, 140);
    CHECK_NEAR(game.enemies[idx].baseSpeed, 170.0f, 0.01f);

    idx = SpawnEnemyAt(ENEMY_ETHEREAL, -60, 140);
    CHECK(game.enemies[idx].energyResist > game.enemies[idx].armor);

    idx = SpawnEnemyAt(ENEMY_HEALER, -60, 140);
    CHECK(game.enemies[idx].goldValue == 40);

    CHECK(id1 < game.enemies[idx].id); // ids strictly increase
}

static void TestSpawnEnemySnap(void) {
    ResetForTest();
    // (100,380) lies exactly on the vertical path segment x=2
    int idx = SpawnEnemyAt(ENEMY_BASIC, 100, 380);
    Enemy* e = &game.enemies[idx];
    CHECK(e->waypointIndex >= 1);
    CHECK(e->waypointIndex < game.map.waypointCount);
    CHECK_NEAR(e->position.x, 100.0f, 0.001);
    CHECK_NEAR(e->position.y, 380.0f, 0.001);

    // pool full -> spawn fails silently
    for (int i = 0; i < MAX_ENEMIES; i++) game.enemies[i].active = true;
    int count = CountActiveEnemies();
    SpawnEnemy(ENEMY_BASIC, (Vector2){100, 100});
    CHECK(CountActiveEnemies() == count);
}

static void TestUpdateEnemiesMovement(void) {
    ResetForTest();
    int idx = SpawnEnemyAt(ENEMY_BASIC, -60, 140);
    Enemy* e = &game.enemies[idx];
    CHECK(e->waypointIndex == 1);
    UpdateEnemies(1.0f); // speed 80 -> x = -60 + 80 = 20
    CHECK_NEAR(e->position.x, 20.0f, 0.01f);
    CHECK(e->waypointIndex == 1);
    UpdateEnemies(4.0f); // walks several waypoints down the path
    CHECK(e->waypointIndex > 1);
    CHECK(e->position.y >= 140.0f);
}

static void TestEnemyLeak(void) {
    ResetForTest();
    int idx = SpawnEnemyAt(ENEMY_BASIC, -60, 140);
    game.enemies[idx].waypointIndex = game.map.waypointCount; // at the end
    int lives0 = game.lives;
    UpdateEnemies(0.1f);
    CHECK(!game.enemies[idx].active);
    CHECK(game.lives == lives0 - 1);
    CHECK(game.screenShakeTime > 0);
}

static void TestHealerAura(void) {
    ResetForTest();
    int healer = SpawnEnemyAt(ENEMY_HEALER, 200, 200);
    int basic = SpawnEnemyAt(ENEMY_BASIC, 230, 200); // within 100
    game.enemies[basic].hp = 60;
    game.enemies[healer].abilityTimer = 0.0f;
    UpdateEnemies(0.1f);
    CHECK(game.enemies[basic].hp > 60.0f);
    CHECK(game.enemies[basic].hp <= game.enemies[basic].maxHp);
    CHECK(game.enemies[healer].abilityTimer > 0);
}

static void TestCalculateDamage(void) {
    ResetForTest();
    int idx = SpawnEnemyAt(ENEMY_BASIC, 100, 100);
    Enemy* e = &game.enemies[idx];
    e->armor = 10;
    e->energyResist = 10;

    CHECK_NEAR(CalculateDamage(100, DMG_PHYSICAL, e), 100.0f * 100.0f / 110.0f, 0.5f);
    CHECK_NEAR(CalculateDamage(100, DMG_ENERGY, e), 100.0f * 100.0f / 110.0f, 0.5f);
    CHECK_NEAR(CalculateDamage(100, DMG_TRUE, e), 100.0f, 0.5f);

    ApplyStatusEffect(e, STATUS_WEAKEN, 5, 1.5f);
    CHECK_NEAR(CalculateDamage(100, DMG_ENERGY, e), 100.0f * 1.5f * 100.0f / 110.0f, 0.5f);
    ApplyStatusEffect(e, STATUS_BRITTLE, 5, 1.2f);
    CHECK_NEAR(CalculateDamage(100, DMG_PHYSICAL, e), 100.0f * 1.2f * 100.0f / 110.0f, 0.5f);
    ApplyStatusEffect(e, STATUS_MELTED_ARMOR, 5, 1.3f);
    // both brittle and melted armor stack: 1.2 * 1.3
    CHECK_NEAR(CalculateDamage(100, DMG_PHYSICAL, e), 100.0f * 1.2f * 1.3f * 100.0f / 110.0f, 0.5f);

    // floor of 0.5 damage
    CHECK_NEAR(CalculateDamage(0.1f, DMG_PHYSICAL, e), 0.5f, 0.001);

    // negative armor edge
    e->statusCount = 0;
    e->armor = -99;
    CHECK_NEAR(CalculateDamage(10, DMG_PHYSICAL, e), 10.0f * 100.0f / 1.0f, 0.5f);
}

static void TestApplyStatusEffect(void) {
    ResetForTest();
    int idx = SpawnEnemyAt(ENEMY_BASIC, 100, 100);
    Enemy* e = &game.enemies[idx];

    ApplyStatusEffect(e, STATUS_NONE, 5, 1.0f);
    CHECK(e->statusCount == 0);

    ApplyStatusEffect(e, STATUS_SLOW, 3.0f, 0.5f);
    CHECK(e->statusCount == 1);
    CHECK(e->status[0].type == STATUS_SLOW);

    // weaker intensity on same type -> keep
    ApplyStatusEffect(e, STATUS_SLOW, 3.0f, 0.4f);
    CHECK(e->status[0].intensity == 0.5f);
    // stronger intensity -> refresh
    ApplyStatusEffect(e, STATUS_SLOW, 3.0f, 0.8f);
    CHECK(e->status[0].intensity == 0.8f);
    // equal intensity, longer duration -> extend
    e->status[0].timer = 2.0f;
    ApplyStatusEffect(e, STATUS_SLOW, 3.0f, 0.8f);
    CHECK(e->status[0].duration == 3.0f);
    CHECK(e->status[0].timer == 0.0f);

    // inactive / null -> no-op
    Enemy* dead = &game.enemies[(idx + 1) % MAX_ENEMIES];
    dead->active = false;
    ApplyStatusEffect(dead, STATUS_BURN, 5, 1.0f);
    CHECK(dead->statusCount == 0);
    ApplyStatusEffect(NULL, STATUS_BURN, 5, 1.0f);

    // boss: durations halved, stun reduced further, slow intensity halved
    int b = SpawnEnemyAt(ENEMY_BOSS, 300, 300);
    Enemy* boss = &game.enemies[b];
    ApplyStatusEffect(boss, STATUS_BURN, 4.0f, 1.0f);
    CHECK_NEAR(boss->status[0].duration, 2.0f, 0.01f);
    ApplyStatusEffect(boss, STATUS_STUN, 4.0f, 1.0f);
    CHECK_NEAR(boss->status[1].duration, 4.0f * 0.5f * 0.3f, 0.01f);
    ApplyStatusEffect(boss, STATUS_SLOW, 4.0f, 1.0f);
    CHECK_NEAR(boss->status[2].intensity, 0.5f, 0.01f);

    // all six distinct statuses fill the pool; refresh still works
    int f = SpawnEnemyAt(ENEMY_FAST, 400, 400);
    Enemy* fill = &game.enemies[f];
    for (int s = STATUS_SLOW; s <= STATUS_MELTED_ARMOR; s <<= 1)
        ApplyStatusEffect(fill, (StatusEffect)s, 5, 1.0f);
    CHECK(fill->statusCount == 6);
    ApplyStatusEffect(fill, STATUS_STUN, 5, 2.0f);
    CHECK(fill->statusCount == 6);
}

static void TestProcessStatusEffects(void) {
    ResetForTest();
    int idx = SpawnEnemyAt(ENEMY_BASIC, 100, 100);
    Enemy* e = &game.enemies[idx];
    float hp0 = e->hp;

    ApplyStatusEffect(e, STATUS_BURN, 1.0f, 10.0f);
    ProcessStatusEffects(e, 0.5f);
    CHECK_NEAR(e->hp, hp0 - 5.0f, 0.5f);
    ProcessStatusEffects(e, 0.6f); // expires
    CHECK(e->statusCount == 0);

    ApplyStatusEffect(e, STATUS_STUN, 1.0f, 1.0f);
    ProcessStatusEffects(e, 0.1f);
    CHECK(e->speed == 0.0f);

    e->statusCount = 0;
    ApplyStatusEffect(e, STATUS_SLOW, 1.0f, 0.5f);
    ProcessStatusEffects(e, 0.1f);
    CHECK_NEAR(e->speed, e->baseSpeed * 0.5f, 0.01f);

    e->statusCount = 0;
    ApplyStatusEffect(e, STATUS_WEAKEN, 1.0f, 1.5f);
    ProcessStatusEffects(e, 0.1f);
    CHECK(e->visualHasOutline);
    CHECK(e->visualOutlineColor.r == COLOR_ENERGY.r);

    e->statusCount = 0;
    ApplyStatusEffect(e, STATUS_MELTED_ARMOR, 1.0f, 1.5f);
    ProcessStatusEffects(e, 0.1f);
    CHECK(e->visualHasOutline);
    CHECK(e->visualOutlineColor.r == COLOR_PHYSICAL.r);

    e->statusCount = 0;
    ApplyStatusEffect(e, STATUS_BRITTLE, 1.0f, 1.5f);
    ProcessStatusEffects(e, 0.1f);
    CHECK(!e->visualHasOutline);

    // middle-status expiry uses swap removal
    e->statusCount = 0;
    ApplyStatusEffect(e, STATUS_BURN, 0.5f, 5.0f);    // expires first
    ApplyStatusEffect(e, STATUS_SLOW, 5.0f, 0.5f);
    ApplyStatusEffect(e, STATUS_WEAKEN, 5.0f, 1.5f);
    ProcessStatusEffects(e, 0.6f);
    CHECK(e->statusCount == 2);
    bool hasSlow = false, hasWeaken = false;
    for (int i = 0; i < e->statusCount; i++) {
        if (e->status[i].type == STATUS_SLOW) hasSlow = true;
        if (e->status[i].type == STATUS_WEAKEN) hasWeaken = true;
    }
    CHECK(hasSlow && hasWeaken);

    // all statuses expire
    ProcessStatusEffects(e, 10.0f);
    CHECK(e->statusCount == 0);
}

static void TestHandleEnemyDeath(void) {
    ResetForTest();
    StubSetRandomValue(0); // basic aether roll: 0 < 5 -> aetherValue 1
    int idx = SpawnEnemyAt(ENEMY_BASIC, 100, 100);
    Enemy* e = &game.enemies[idx];
    CHECK(e->aetherValue == 1);
    int gold0 = game.gold, aether0 = game.aether, xp0 = game.hero.xp;
    HandleEnemyDeath(idx, -1);
    CHECK(game.gold == gold0 + e->goldValue);
    CHECK(game.aether == aether0 + e->aetherValue);
    CHECK(game.hero.xp == xp0 + (int)(e->xpValue * 0.4f)); // non-hero source
    CHECK(!e->active);

    // invalid / already-dead -> no-op
    int gold1 = game.gold;
    HandleEnemyDeath(-5, -1);
    HandleEnemyDeath(idx, -1);
    CHECK(game.gold == gold1);

    // tower kill credit
    int t = SpawnEnemyAt(ENEMY_FAST, 200, 200);
    PlaceTower(5, 5, TOWER_PULSE);
    HandleEnemyDeath(t, 0);
    CHECK(game.towers[0].kills == 1);
    CHECK(game.towers[0].stats.xp > 0);

    // hero kill gives full xp (source -2)
    int h = SpawnEnemyAt(ENEMY_MINION, 300, 300);
    int minionXp = game.enemies[h].xpValue;
    int hxp0 = game.hero.xp;
    HandleEnemyDeath(h, -2);
    CHECK(game.hero.xp == hxp0 + minionXp);
}

void TestEnemies(void) {
    TestGrid();
    TestGetEnemiesInRadius();
    TestSpawnEnemyTypes();
    TestSpawnEnemySnap();
    TestUpdateEnemiesMovement();
    TestEnemyLeak();
    TestHealerAura();
    TestCalculateDamage();
    TestApplyStatusEffect();
    TestProcessStatusEffects();
    TestHandleEnemyDeath();
}
