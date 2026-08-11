// Tests for towers.c: stats, placement, sale, upgrades, targeting, firing, XP.
#include "test_util.h"

static Tower* TowerAt(int i) { return &game.towers[i]; }

static void TestConfigureTowerStats(void) {
    ResetForTest();
    for (int t = TOWER_PULSE; t <= TOWER_T4_TESLA_STORM; t++) {
        Tower tower = {0};
        tower.type = (TowerType)t;
        tower.stats.level = 1;
        ConfigureTowerStats(&tower);
        CHECK(tower.stats.damage > 0);
        CHECK(tower.stats.range > 0);
        CHECK(tower.stats.fireRate >= 0);
        if (t >= TOWER_T4_PULSE_REPEATER)
            CHECK(tower.stats.xpToNextLevel == 99999);
    }

    Tower tower = {0};
    tower.type = TOWER_PULSE;
    tower.stats.level = 1;
    ConfigureTowerStats(&tower);
    CHECK(tower.stats.xpToNextLevel == 150);
    CHECK(tower.stats.damage == 20);
    CHECK_NEAR(tower.stats.range, 150.0f, 0.01f);
    CHECK_NEAR(tower.stats.fireRate, 2.5f, 0.01f);
    CHECK(tower.damageType == DMG_ENERGY);

    // level scaling
    tower.stats.level = 2;
    ConfigureTowerStats(&tower);
    CHECK_NEAR(tower.stats.damage, 20.0f * 1.35f, 0.01f);
    CHECK_NEAR(tower.stats.range, 150.0f * 1.10f, 0.01f);
    CHECK_NEAR(tower.stats.fireRate, 2.5f * 1.15f, 0.01f);

    // cryo keeps fireRate 0 at any level
    Tower cryo = {0};
    cryo.type = TOWER_CRYO;
    cryo.stats.level = 3;
    ConfigureTowerStats(&cryo);
    CHECK(cryo.stats.fireRate == 0.0f);

    // unknown type -> untouched
    Tower none = {0};
    none.type = TOWER_NONE;
    none.stats.level = 1;
    ConfigureTowerStats(&none);
    CHECK(none.stats.damage == 0);
    CHECK(none.stats.range == 0);
}

static void TestPlaceTower(void) {
    ResetForTest();
    CHECK(PlaceTower(5, 5, TOWER_PULSE));
    CHECK(CountActiveTowers() == 1);
    CHECK(game.gold == 250);
    CHECK(game.occupied[5][5]);
    Tower* t = TowerAt(0);
    CHECK(t->type == TOWER_PULSE);
    CHECK(t->stats.level == 1);
    CHECK(t->targetingMode == TARGET_FIRST);
    CHECK(t->totalCost == 100);
    CHECK(t->targetIndex == -1);
    CHECK_NEAR(t->position.x, 220.0f, 0.01f);
    CHECK_NEAR(t->position.y, 220.0f, 0.01f);

    CHECK(!PlaceTower(5, 5, TOWER_CANNON));  // occupied
    CHECK(!PlaceTower(0, 3, TOWER_PULSE));   // path tile
    CHECK(!PlaceTower(-1, 0, TOWER_PULSE));  // out of bounds
    game.gold = 50;
    CHECK(!PlaceTower(3, 4, TOWER_TESLA));   // cannot afford
    CHECK(game.gold == 50);
}

static void TestSellTower(void) {
    ResetForTest();
    PlaceTower(5, 5, TOWER_PULSE);
    int gold0 = game.gold;
    game.selectedTowerIndex = 0;
    SellTower(0);
    CHECK(game.gold == gold0 + 60); // 100 * 0.6
    CHECK(!game.towers[0].active);
    CHECK(!game.occupied[5][5]);
    CHECK(game.selectedTowerIndex == -1);

    int gold1 = game.gold;
    SellTower(-1);
    SellTower(MAX_TOWERS);
    CHECK(game.gold == gold1);
}

static void TestUpgradeTower(void) {
    ResetForTest();
    game.gold = 10000;
    // tiles (6,5) etc. are on the path, so grab each tower's index right after
    // placing it instead of assuming fixed slots.
    CHECK(PlaceTower(4, 4, TOWER_PULSE));
    int p = CountActiveTowers() - 1;
    CHECK(UpgradeTower(TowerAt(p), TOWER_T4_PULSE_REPEATER));
    CHECK(TowerAt(p)->type == TOWER_T4_PULSE_REPEATER);
    CHECK_NEAR(TowerAt(p)->stats.fireRate, 8.0f, 0.01f);

    CHECK(PlaceTower(5, 4, TOWER_CANNON));
    int c = CountActiveTowers() - 1;
    CHECK(UpgradeTower(TowerAt(c), TOWER_T4_CANNON_MORTAR));
    CHECK(TowerAt(c)->type == TOWER_T4_CANNON_MORTAR);

    CHECK(PlaceTower(7, 4, TOWER_CRYO));
    int cr = CountActiveTowers() - 1;
    CHECK(UpgradeTower(TowerAt(cr), TOWER_T4_CRYO_BLIZZARD));
    CHECK(TowerAt(cr)->type == TOWER_T4_CRYO_BLIZZARD);

    CHECK(PlaceTower(8, 4, TOWER_TESLA));
    int te = CountActiveTowers() - 1;
    CHECK(UpgradeTower(TowerAt(te), TOWER_T4_TESLA_STORM));
    CHECK(TowerAt(te)->type == TOWER_T4_TESLA_STORM);

    // wrong path
    CHECK(!UpgradeTower(TowerAt(p), TOWER_T4_TESLA_CHAIN));
    // null / inactive
    CHECK(!UpgradeTower(NULL, TOWER_T4_PULSE_REPEATER));
    Tower dummy = {0};
    CHECK(!UpgradeTower(&dummy, TOWER_T4_PULSE_REPEATER));
}

static void TestUpdateTowersFiring(void) {
    ResetForTest();
    PlaceTower(5, 5, TOWER_PULSE); // (220,220)
    Tower* t = TowerAt(0);
    int e = SpawnEnemyAt(ENEMY_BASIC, 220, 300);
    RebuildEnemyGrid();
    t->targetIndex = e;
    t->targetEnemyId = game.enemies[e].id;
    t->rotation = 90.0f;
    t->desiredRotation = 90.0f;
    UpdateTowers(0.1f);
    CHECK(CountActiveProjectiles() == 1);
    Projectile* p = &game.projectiles[0];
    CHECK(p->active);
    CHECK_NEAR(p->speed, 650.0f, 0.01f);
    CHECK_NEAR(p->damage, 20.0f, 0.01f);
    CHECK(p->damageType == DMG_ENERGY);
    CHECK(p->targetEnemyId == game.enemies[e].id);
    CHECK(p->sourceTowerIndex == 0);
    CHECK_NEAR(t->cooldownTimer, 0.4f, 0.01f); // 1/2.5

    UpdateTowers(0.1f); // still cooling down -> no second shot
    CHECK(CountActiveProjectiles() == 1);
}

static void TestTargetAcquisition(void) {
    ResetForTest();
    PlaceTower(5, 5, TOWER_PULSE);
    Tower* t = TowerAt(0);
    int e = SpawnEnemyAt(ENEMY_BASIC, 220, 300);
    RebuildEnemyGrid();
    t->targetSearchTimer = 0.0f;
    UpdateTowers(0.1f);
    CHECK(t->targetIndex == e);
    CHECK(t->targetEnemyId == game.enemies[e].id);

    // target dies -> dropped, and search finds nothing
    game.enemies[e].active = false;
    RebuildEnemyGrid();
    t->targetSearchTimer = 0.0f;
    UpdateTowers(0.1f);
    CHECK(t->targetIndex == -1);
    CHECK(t->targetEnemyId == -1);
}

static void TestTargetingModes(void) {
    ResetForTest();
    PlaceTower(5, 5, TOWER_PULSE);
    Tower* t = TowerAt(0);
    t->stats.range = 500.0f;
    int near = SpawnEnemyAt(ENEMY_BASIC, 220, 300);
    int mid = SpawnEnemyAt(ENEMY_BASIC, 220, 400);
    int far = SpawnEnemyAt(ENEMY_BASIC, 220, 600);
    game.enemies[near].waypointIndex = 2;
    game.enemies[mid].waypointIndex = 3;
    game.enemies[far].waypointIndex = 5;
    game.enemies[near].hp = 900;
    game.enemies[near].maxHp = 900;
    game.enemies[mid].hp = 500;
    game.enemies[mid].maxHp = 500;
    game.enemies[far].hp = 100;
    game.enemies[far].maxHp = 100;
    RebuildEnemyGrid();

    t->targetingMode = TARGET_FIRST;
    t->targetIndex = -1; t->targetEnemyId = -1; t->targetSearchTimer = 0;
    UpdateTowers(0.1f);
    CHECK(t->targetIndex == far); // furthest along the path

    t->targetingMode = TARGET_CLOSEST;
    t->targetIndex = -1; t->targetEnemyId = -1; t->targetSearchTimer = 0;
    UpdateTowers(0.1f);
    CHECK(t->targetIndex == near);

    t->targetingMode = TARGET_STRONGEST;
    t->targetIndex = -1; t->targetEnemyId = -1; t->targetSearchTimer = 0;
    UpdateTowers(0.1f);
    CHECK(t->targetIndex == near); // hp 900

    t->targetingMode = TARGET_WEAKEST;
    t->targetIndex = -1; t->targetEnemyId = -1; t->targetSearchTimer = 0;
    UpdateTowers(0.1f);
    CHECK(t->targetIndex == far); // hp 100
}

static void TestCryoBeam(void) {
    ResetForTest();
    PlaceTower(5, 5, TOWER_CRYO);
    Tower* t = TowerAt(0);
    int e = SpawnEnemyAt(ENEMY_BASIC, 220, 300);
    RebuildEnemyGrid();
    t->targetIndex = e;
    t->targetEnemyId = game.enemies[e].id;
    t->rotation = 90.0f;
    t->desiredRotation = 90.0f;
    float hp0 = game.enemies[e].hp;
    UpdateTowers(0.1f);
    CHECK(game.enemies[e].hp < hp0);
    CHECK(game.enemies[e].statusCount >= 2); // slow + brittle
    CHECK(CountActiveProjectiles() == 0);
}

static void TestFreezer(void) {
    ResetForTest();
    game.gold = 5000; game.aether = 200;
    PlaceTower(5, 5, TOWER_CRYO);
    Tower* t = TowerAt(0);
    UpgradeTower(t, TOWER_T4_CRYO_FREEZER);
    CHECK_NEAR(t->stats.fireRate, 1.5f, 0.01f);
    int e = SpawnEnemyAt(ENEMY_BASIC, 220, 300);
    RebuildEnemyGrid();
    t->targetIndex = e;
    t->targetEnemyId = game.enemies[e].id;
    t->rotation = 90.0f;
    t->desiredRotation = 90.0f;
    UpdateTowers(0.1f);
    bool stunned = false;
    for (int i = 0; i < game.enemies[e].statusCount; i++)
        if (game.enemies[e].status[i].type == STATUS_STUN) stunned = true;
    CHECK(stunned);
    CHECK_NEAR(t->cooldownTimer, 1.0f / 1.5f, 0.01f);
}

static void TestBlizzard(void) {
    ResetForTest();
    game.gold = 5000; game.aether = 200;
    PlaceTower(5, 5, TOWER_CRYO);
    Tower* t = TowerAt(0);
    UpgradeTower(t, TOWER_T4_CRYO_BLIZZARD);
    int e = SpawnEnemyAt(ENEMY_BASIC, 220, 300); // dist 80 < range 120
    RebuildEnemyGrid();
    float hp0 = game.enemies[e].hp;
    UpdateTowers(0.1f);
    CHECK(game.enemies[e].hp < hp0);
    bool slowed = false, brittle = false;
    for (int i = 0; i < game.enemies[e].statusCount; i++) {
        if (game.enemies[e].status[i].type == STATUS_SLOW) slowed = true;
        if (game.enemies[e].status[i].type == STATUS_BRITTLE) brittle = true;
    }
    CHECK(slowed && brittle);

    game.enemies[e].hp = 0.5f; // aura finishes it off
    UpdateTowers(0.1f);
    CHECK(!game.enemies[e].active);
}

static void TestMortar(void) {
    ResetForTest();
    game.gold = 5000; game.aether = 200;
    PlaceTower(5, 5, TOWER_CANNON);
    Tower* t = TowerAt(0);
    UpgradeTower(t, TOWER_T4_CANNON_MORTAR);
    SpawnEnemyAt(ENEMY_BASIC, 220, 300);
    RebuildEnemyGrid();
    UpdateTowers(0.1f); // global range: fires even without aim alignment
    CHECK(CountActiveProjectiles() == 1);
    Projectile* p = &game.projectiles[0];
    CHECK(p->sourceType == TOWER_T4_CANNON_MORTAR);
    CHECK_NEAR(p->speed, 250.0f, 0.01f);
    CHECK_NEAR(p->aoeRadius, 150.0f, 0.01f);
    CHECK_NEAR(p->lifetime, 6.0f, 0.01f);
    CHECK(p->applyStatus == STATUS_BURN);
    CHECK_NEAR(p->statusIntensity, 15.0f, 0.01f);
    CHECK_NEAR(p->statusDuration, 5.0f, 0.01f);
}

static void TestTowerIdle(void) {
    ResetForTest();
    PlaceTower(5, 5, TOWER_PULSE);
    Tower* t = TowerAt(0);
    t->rotation = 45.0f;
    RebuildEnemyGrid();
    UpdateTowers(0.5f); // no enemies: rotation eases back to 0
    CHECK_NEAR(t->rotation, 0.0f, 2.0f);
}

static void TestGrantXP(void) {
    ResetForTest();
    PlaceTower(5, 5, TOWER_PULSE);
    Tower* t = TowerAt(0);
    GrantXP(0, 100);
    CHECK(t->stats.xp == 100);
    GrantXP(0, 100); // 200 >= 150 -> level 2
    CHECK(t->stats.level == 2);
    CHECK(t->stats.xp == 50);
    CHECK(t->stats.xpToNextLevel == 285);
    CHECK_NEAR(t->stats.damage, 20.0f * 1.35f, 0.01f);

    GrantXP(-1, 100);
    GrantXP(5, 100); // inactive index
    CHECK(t->stats.xp == 50);

    // T4 towers stop gaining xp
    game.gold = 5000; game.aether = 200;
    UpgradeTower(t, TOWER_T4_PULSE_REPEATER);
    GrantXP(0, 500);
    CHECK(t->stats.xp == 50);

    // leadership aura bonus when hero is near
    game.hero.skills[SKILL_LEADERSHIP] = 2;
    game.hero.position = (Vector2){140, 180};
    PlaceTower(3, 4, TOWER_CANNON); // (140,180)
    GrantXP(1, 60); // + 60 * 0.15 * 2 = +18 -> 78 (kept under the 150 level-up)
    CHECK(game.towers[1].stats.xp == 78);
    // hero far away -> no bonus
    game.hero.position = (Vector2){900, 700};
    GrantXP(1, 60);
    CHECK(game.towers[1].stats.xp == 138);
}

static void TestLevelUpTowerDirect(void) {
    ResetForTest();
    PlaceTower(5, 5, TOWER_PULSE);
    Tower* t = TowerAt(0);
    t->stats.xp = 150;
    LevelUpTower(0);
    CHECK(t->stats.level == 2);
    CHECK(t->stats.xp == 0);
    CHECK(t->stats.xpToNextLevel == 285);
}

void TestTowers(void) {
    TestConfigureTowerStats();
    TestPlaceTower();
    TestSellTower();
    TestUpgradeTower();
    TestUpdateTowersFiring();
    TestTargetAcquisition();
    TestTargetingModes();
    TestCryoBeam();
    TestFreezer();
    TestBlizzard();
    TestMortar();
    TestTowerIdle();
    TestGrantXP();
    TestLevelUpTowerDirect();
}
