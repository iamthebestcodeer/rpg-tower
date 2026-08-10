// Tests for draw.c and ui.c: rendering paths and UI interactions.
#include "test_util.h"

static Tower* TowerAt(int i) { return &game.towers[i]; }

// True if a logged DrawText call drew `text` at exactly (x, y).
static bool StubFindTextAt(const char* text, int x, int y) {
    for (int i = 0; i < StubDrawLogCount(); i++) {
        StubDrawCall c = StubDrawLogAt(i);
        if (c.kind == STUB_DRAW_TEXT && (int)c.x == x && (int)c.y == y &&
            strcmp(c.text, text) == 0)
            return true;
    }
    return false;
}

static void TestDrawGameStates(void) {
    ResetForTest();
    game.state = GS_TITLE;
    game.globalTime = 1.0f;
    DrawGame();

    ResetForTest();
    game.state = GS_PLAYING;
    PlaceTower(5, 5, TOWER_PULSE);
    SpawnEnemyAt(ENEMY_BASIC, 220, 300);
    SpawnParticles((Vector2){100, 100}, 5, WHITE, BLACK, 50.0f, 4.0f, 1.0f, false);
    AddFloatingText((Vector2){150, 150}, "TEXT", WHITE, false);
    game.globalTime = 2.0f;
    DrawGame();

    ResetForTest();
    game.state = GS_PAUSED;
    DrawGame();
    CHECK(StubFindText("PAUSED (Press P)") >= 0);

    ResetForTest();
    game.state = GS_GAME_OVER;
    game.currentWave = 4;
    DrawGame();
    CHECK(StubFindText("GAME OVER") >= 0);
}

static void TestDrawLevelUpSkillClick(void) {
    ResetForTest();
    game.state = GS_LEVEL_UP_HERO;
    game.hero.skillPoints = 1;
    // panel {320,200,640,400}; first skill button {340,300,600,50}
    StubSetMousePosition(350, 320);
    StubClickMouse(MOUSE_LEFT_BUTTON);
    DrawGame();
    CHECK(game.hero.skillPoints == 1); // mutation deferred to update
    UpdateHeroLevelUp(0.1f);
    CHECK(game.hero.skillPoints == 0);
    CHECK(game.hero.skills[SKILL_VIGOR] == 1);
    CHECK(game.hero.attackDamage == 50); // 35 + 15
    CHECK(game.state == GS_PLAYING);
}

static void TestDrawMap(void) {
    ResetForTest();
    DrawMap(); // already built
    game.mapRTBuilt = false;
    DrawMap(); // rebuild path
    CHECK(game.mapRTBuilt);
}

static void TestDrawEnemies(void) {
    ResetForTest();
    int types[] = {ENEMY_BASIC, ENEMY_FAST, ENEMY_TANK, ENEMY_ETHEREAL,
                   ENEMY_HEALER, ENEMY_SPAWNER, ENEMY_MINION, ENEMY_BOSS};
    for (int i = 0; i < 8; i++) {
        int idx = SpawnEnemyAt((EnemyType)types[i], 100 + i * 60, 200);
        game.enemies[idx].hp = game.enemies[idx].maxHp * 0.5f; // health bar
        if (i == 0) {
            ApplyStatusEffect(&game.enemies[idx], STATUS_STUN, 5, 1.0f); // stun text
            game.enemies[idx].visualHasOutline = true;                  // outline
            game.enemies[idx].visualOutlineColor = COLOR_ENERGY;
        }
        if (i == 1) game.enemies[idx].hp = game.enemies[idx].maxHp; // full hp: no bar
    }
    game.globalTime = 1.0f;
    DrawEnemies();
    // Every damaged enemy gets a partial health-bar fill (height 6, width
    // exactly half the bar: 20, or 30 for the boss); the full-HP fast enemy
    // gets none, so exactly 7 fills are logged.
    int bars = 0;
    for (int i = 0; i < StubDrawLogCount(); i++) {
        StubDrawCall c = StubDrawLogAt(i);
        if (c.kind == STUB_DRAW_RECT && fabsf(c.h - 6.0f) <= 0.5f && c.w > 0.5f && c.w < 39.5f) bars++;
    }
    CHECK(bars == 7);
}

static void TestDrawTowers(void) {
    ResetForTest();
    game.gold = 50000; game.aether = 500;
    TowerType order[] = {TOWER_PULSE, TOWER_CANNON, TOWER_CRYO, TOWER_TESLA,
                         TOWER_T4_PULSE_REPEATER, TOWER_T4_PULSE_SNIPER,
                         TOWER_T4_CANNON_MORTAR, TOWER_T4_CANNON_VULCAN,
                         TOWER_T4_CRYO_BLIZZARD, TOWER_T4_CRYO_FREEZER,
                         TOWER_T4_TESLA_CHAIN, TOWER_T4_TESLA_STORM};
    int xs[] = {4, 5, 7, 8}; // rows 0-2 are fully buildable (no path tiles)
    int ys[] = {0, 1, 2};
    for (int i = 0; i < 12; i++) {
        int x = xs[i % 4], y = ys[i / 4];
        TowerType base = TOWER_PULSE;
        switch (order[i]) {
            case TOWER_CANNON: case TOWER_T4_CANNON_MORTAR: case TOWER_T4_CANNON_VULCAN:
                base = TOWER_CANNON; break;
            case TOWER_CRYO: case TOWER_T4_CRYO_BLIZZARD: case TOWER_T4_CRYO_FREEZER:
                base = TOWER_CRYO; break;
            case TOWER_TESLA: case TOWER_T4_TESLA_CHAIN: case TOWER_T4_TESLA_STORM:
                base = TOWER_TESLA; break;
            default: break;
        }
        CHECK(PlaceTower(x, y, base));
        int idx = CountActiveTowers() - 1;
        if (order[i] >= TOWER_T4_PULSE_REPEATER)
            CHECK(UpgradeTower(&game.towers[idx], order[i]));
    }

    // align a cryo tower with an enemy so the beam-draw path runs. SpawnEnemy
    // snaps off-path spawns onto the path waypoints, so pin the enemy exactly
    // below the tower (matching rotation 90) to keep the beam deterministic.
    Vector2 cryoPos = {0};
    for (int i = 0; i < MAX_TOWERS; i++) {
        if (game.towers[i].active && game.towers[i].type == TOWER_CRYO) {
            Tower* t = &game.towers[i];
            cryoPos = t->position;
            int e = SpawnEnemyAt(ENEMY_BASIC, t->position.x, t->position.y + 60);
            game.enemies[e].position = (Vector2){t->position.x, t->position.y + 60.0f};
            RebuildEnemyGrid();
            t->targetIndex = e;
            t->targetEnemyId = game.enemies[e].id;
            t->rotation = 90.0f;
            t->desiredRotation = 90.0f;
            break;
        }
    }
    // deterministic tesla sparkle for a tesla-family tower (index 3 -> 7*3=21;
    // need sparkSeed % 16 == 0 -> (int)(globalTime*60) % 16 == 11)
    game.globalTime = 11.0f / 60.0f;
    DrawTowers();

    // cryo beam (+ glow) drawn from the aligned tower to its target
    CHECK(StubFindLine(cryoPos.x, cryoPos.y, cryoPos.x, cryoPos.y + 60.0f, 0.01f) >= 0);
    // tesla sparkle: tower index 3 at tile (8,0), sparkSeed 11 + 3*7 = 32 ->
    // 32 % 16 == 0, so the spark line is drawn at angle (32 + 3*13) % 360 = 71
    Vector2 sparkStart = TileToWorldCenter(8, 0);
    float sparkAngle = (float)((32 + 3 * 13) % 360) * DEG2RAD;
    Vector2 sparkEnd = { sparkStart.x + cosf(sparkAngle) * 22.0f,
                         sparkStart.y + sinf(sparkAngle) * 22.0f };
    CHECK(StubFindLine(sparkStart.x, sparkStart.y, sparkEnd.x, sparkEnd.y, 0.01f) >= 0);
}

static void TestDrawHero(void) {
    ResetForTest();
    // hero starts at (100,100): body circle r=15 + outline, no aura yet
    DrawHero();
    CHECK(StubFindCircle(STUB_DRAW_CIRCLE_FILL, 100.0f, 100.0f, 150.0f, 0.5f) < 0);

    game.hero.skills[SKILL_LEADERSHIP] = 1;
    DrawHero();
    // leadership aura: 150-radius circle centered on the hero
    CHECK(StubFindCircle(STUB_DRAW_CIRCLE_FILL, 100.0f, 100.0f, 150.0f, 0.5f) >= 0);
}

static void TestDrawProjectiles(void) {
    ResetForTest();
    game.gold = 5000; game.aether = 200;
    PlaceTower(5, 5, TOWER_CANNON);
    Tower* t = TowerAt(0);
    int e = SpawnEnemyAt(ENEMY_BASIC, 220, 300);
    t->targetIndex = e; t->targetEnemyId = game.enemies[e].id;
    t->rotation = 90.0f; t->desiredRotation = 90.0f;
    UpdateTowers(0.1f); // physical cannon shell

    PlaceTower(7, 5, TOWER_PULSE); // (6,5) is a path tile
    Tower* t2 = TowerAt(1);
    t2->targetIndex = e; t2->targetEnemyId = game.enemies[e].id;
    t2->rotation = 90.0f; t2->desiredRotation = 90.0f;
    UpdateTowers(0.1f); // energy bolt (glow fan)

    DrawProjectiles();
    // single batched fan pass: cannon shell (physical, no glow) + pulse bolt
    // (energy) + its glow ring => 3 fans of 36 triangles x 3 vertices
    CHECK(StubRlBeginCount() == 1);
    CHECK(StubRlVertexCount() == 3 * 36 * 3);
}

static void TestDrawUIBasics(void) {
    ResetForTest();
    StubSetMousePosition(100, 100);
    DrawUI(true); // build menu

    // placement range highlight: tile (5,5) center (220,220), pulse range 150
    game.placingTower = TOWER_PULSE;
    StubSetMousePosition(200, 200);
    StubResetDrawLog();
    DrawUI(true);
    CHECK(StubFindCircle(STUB_DRAW_CIRCLE_LINE, 220.0f, 220.0f, 150.0f, 0.5f) >= 0);
    CHECK(StubFindCircle(STUB_DRAW_CIRCLE_FILL, 220.0f, 220.0f, 150.0f, 0.5f) >= 0);
    game.placingTower = TOWER_NONE;

    // selected tower range circle at the same position/radius
    PlaceTower(5, 5, TOWER_PULSE);
    game.selectedTowerIndex = 0;
    StubSetMousePosition(100, 100);
    StubResetDrawLog();
    DrawUI(true); // inspector
    CHECK(StubFindCircle(STUB_DRAW_CIRCLE_LINE, 220.0f, 220.0f, 150.0f, 0.5f) >= 0);
    CHECK(StubFindCircle(STUB_DRAW_CIRCLE_FILL, 220.0f, 220.0f, 150.0f, 0.5f) >= 0);
}

static void TestGuiButton(void) {
    ResetForTest();
    Rectangle r = {100, 100, 200, 50};
    StubSetMousePosition(150, 125);
    StubClickMouse(MOUSE_LEFT_BUTTON);
    CHECK(GuiButton(r, "Click", false, true)); // enabled + hovered + clicked

    StubResetInput();
    StubSetMousePosition(150, 125);
    StubClickMouse(MOUSE_LEFT_BUTTON);
    CHECK(!GuiButton(r, "Click", false, false)); // disabled

    StubResetInput();
    StubSetMousePosition(50, 50); // outside
    StubClickMouse(MOUSE_LEFT_BUTTON);
    CHECK(!GuiButton(r, "Click", false, true)); // not hovered

    StubResetInput();
    StubSetMousePosition(150, 125);
    CHECK(!GuiButton(r, "Click", false, true)); // hovered but not clicked

    StubResetInput();
    StubSetMousePosition(150, 125);
    StubClickMouse(MOUSE_LEFT_BUTTON);
    CHECK(GuiButton(r, "Click", true, true)); // selected variant
}

static void TestSetTooltipAndDraw(void) {
    ResetForTest();
    Rectangle r = {100, 100, 50, 30};
    StubSetMousePosition(120, 115); // inside
    SetTooltip("T", "D", r);
    CHECK(game.tooltip.visible);
    CHECK_STREQ(game.tooltip.title, "T");

    game.tooltip.visible = false;
    StubSetMousePosition(500, 500); // outside
    SetTooltip("T", "D", r);
    CHECK(!game.tooltip.visible);

    game.tooltip.visible = true;
    game.tooltip.title = "Title";
    game.tooltip.description = "Line one\nLine two";
    StubSetMousePosition(100, 100);
    DrawTooltip();
    // box {115,115,96,97}: title at (125,125), description lines at y+45/+66
    CHECK(StubFindTextAt("Title", 125, 125));
    CHECK(StubFindTextAt("Line one", 125, 160));
    CHECK(StubFindTextAt("Line two", 125, 181));

    StubSetMousePosition(1270, 790); // near edges -> box clamps to screen edge
    DrawTooltip();
    // width = 76+2*10 = 96 -> x = 1280-96-5 = 1179; height = 25+42+30 = 97 ->
    // y = 790-97-15 = 678, so the title lands at (1189, 688) and the first
    // description line inside the clamped box at (1189, 723)
    CHECK(StubFindTextAt("Title", 1179 + 10, 678 + 10));
    CHECK(StubFindTextAt("Line one", 1179 + 10, 678 + 45));
    game.tooltip.visible = false;
    DrawTooltip(); // early return
}

static void TestDrawBuildMenu(void) {
    ResetForTest();
    StubSetMousePosition(1000, 320); // button 0 {970,300,300,65}
    StubClickMouse(MOUSE_LEFT_BUTTON);
    DrawUI(true);
    CHECK(game.placingTower == TOWER_PULSE);

    // unaffordable: click ignored. NOTE: under the stub's consume-once
    // IsMouseButtonPressed, GuiButton eats the press edge first, so the
    // "Not enough Gold!" floating-text branch inside DrawBuildMenu is never
    // covered by this suite. In real raylib the press edge is frame-wide (not
    // consumed on read), so that branch does fire in the actual game; this
    // test documents the stub-world behavior only.
    ResetForTest();
    game.gold = 10;
    StubSetMousePosition(1000, 320);
    StubClickMouse(MOUSE_LEFT_BUTTON);
    DrawUI(true);
    CHECK(game.placingTower == TOWER_NONE);

    // non-interactive: clicks ignored
    ResetForTest();
    StubSetMousePosition(1000, 320);
    StubClickMouse(MOUSE_LEFT_BUTTON);
    DrawUI(false);
    CHECK(game.placingTower == TOWER_NONE);
}

static void TestDrawTowerInspector(void) {
    ResetForTest();
    PlaceTower(5, 5, TOWER_PULSE);
    game.selectedTowerIndex = 0;
    StubSetMousePosition(100, 100);
    DrawUI(true);
    CHECK(game.selectedTowerIndex == 0);

    // invalid selection resets
    game.selectedTowerIndex = 7;
    DrawUI(true);
    CHECK(game.selectedTowerIndex == -1);
}

static void TestInspectorInteractions(void) {
    ResetForTest();
    PlaceTower(5, 5, TOWER_PULSE);
    game.selectedTowerIndex = 0;
    // targeting mode button: {970, 545, 300, 40}
    StubSetMousePosition(1000, 560);
    StubClickMouse(MOUSE_LEFT_BUTTON);
    DrawUI(true);
    CHECK(game.towers[0].targetingMode == TARGET_CLOSEST);

    // sell button: {970, 750, 300, 40}
    int gold0 = game.gold;
    StubSetMousePosition(1000, 765);
    StubClickMouse(MOUSE_LEFT_BUTTON);
    DrawUI(true);
    CHECK(!game.towers[0].active);
    CHECK(game.gold == gold0 + 60);
    CHECK(game.selectedTowerIndex == -1);
}

static void TestUpgradePaths(void) {
    ResetForTest();
    game.gold = 5000; game.aether = 200;
    PlaceTower(5, 5, TOWER_PULSE);
    GrantXP(0, 1000);
    CHECK(game.towers[0].stats.level == TOWER_BASE_MAX_LEVEL);
    game.selectedTowerIndex = 0;
    int gold0 = game.gold, aether0 = game.aether;
    // upgrade path button 0: {970, 630, 300, 70}
    StubSetMousePosition(1000, 660);
    StubClickMouse(MOUSE_LEFT_BUTTON);
    DrawUI(true);
    CHECK(game.towers[0].type == TOWER_T4_PULSE_REPEATER);
    CHECK(game.gold == gold0 - 800);
    CHECK(game.aether == aether0 - 90);
    CHECK(game.towers[0].totalCost == 100 + 800);
}

static void TestUpgradePathsNonBase(void) {
    ResetForTest();
    game.gold = 5000; game.aether = 200;
    PlaceTower(5, 5, TOWER_PULSE);
    UpgradeTower(&game.towers[0], TOWER_T4_PULSE_REPEATER);
    game.selectedTowerIndex = 0;
    DrawUI(true); // specialized: no xp bar / upgrade paths
    DrawTowerUpgradePaths(&game.towers[0], true); // non-base -> early return

    // tier-4 tower: specialized inspector shown. The paths header is drawn
    // before the type switch, but the base-only upgrade buttons (the actual
    // controls) must be absent.
    CHECK(StubFindText("Specialized Tower (Max Tier)") >= 0);
    CHECK(StubFindText("T4: Pulse Repeater (800G, 90A)") < 0);
    CHECK(StubFindText("T4: Marksman Laser (1000G, 100A)") < 0);
}

static void TestBlizzardInspector(void) {
    ResetForTest();
    game.gold = 5000; game.aether = 200;
    PlaceTower(5, 5, TOWER_CRYO);
    UpgradeTower(&game.towers[0], TOWER_T4_CRYO_BLIZZARD);
    game.selectedTowerIndex = 0;
    StubSetMousePosition(100, 100);
    DrawUI(true); // blizzard inspector: no targeting button
    CHECK(StubFindText("Slow Aura: 75%") >= 0);     // blizzard-specific stat shown
    CHECK(StubFindText("Targeting Mode:") < 0);     // targeting control omitted
}

void TestDrawUI(void) {
    TestDrawGameStates();
    TestDrawLevelUpSkillClick();
    TestDrawMap();
    TestDrawEnemies();
    TestDrawTowers();
    TestDrawHero();
    TestDrawProjectiles();
    TestDrawUIBasics();
    TestGuiButton();
    TestSetTooltipAndDraw();
    TestDrawBuildMenu();
    TestDrawTowerInspector();
    TestInspectorInteractions();
    TestUpgradePaths();
    TestUpgradePathsNonBase();
    TestBlizzardInspector();
}
