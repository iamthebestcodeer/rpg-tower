// Tests for hero.c: skills, movement, dash, attack, burst, XP/leveling.
#include "test_util.h"

static void TestApplyHeroSkills(void) {
    ResetForTest();
    Hero* h = &game.hero;
    CHECK(h->level == 1);
    CHECK(h->attackDamage == 35);
    CHECK_NEAR(h->speed, 180.0f, 0.01f);
    CHECK_NEAR(h->dashCooldown, 6.0f, 0.01f);
    CHECK_NEAR(h->burstDamage, 100.0f, 0.01f);
    CHECK_NEAR(h->burstRange, 120.0f, 0.01f);

    h->level = 3;
    h->skills[SKILL_VIGOR] = 2;
    h->skills[SKILL_AGILITY] = 3;
    h->skills[SKILL_BURST_MASTERY] = 2;
    ApplyHeroSkills();
    CHECK(h->attackDamage == 35 + 2 * 10 + 2 * 15); // 85
    CHECK_NEAR(h->speed, 190.0f * 1.3f, 0.01f);
    CHECK_NEAR(h->dashCooldown, 6.0f * powf(0.95f, 2) * powf(0.9f, 3), 0.01f);
    CHECK_NEAR(h->burstDamage, 150.0f * 1.4f, 0.01f);
    CHECK_NEAR(h->burstRange, 130.0f * 1.2f, 0.01f);
}

static void TestUpdateHeroMovement(void) {
    ResetForTest();
    Hero* h = &game.hero;
    h->position = (Vector2){200, 200};
    StubSetKeyDown(KEY_W, true);
    UpdateHero(1.0f);
    CHECK_NEAR(h->position.x, 200.0f, 0.01f);
    CHECK_NEAR(h->position.y, 20.0f, 0.01f); // 200 - 180
    CHECK(h->lastMovementDirection.y < 0);

    StubResetInput();
    h->position = (Vector2){200, 200};
    StubSetKeyDown(KEY_A, true);
    UpdateHero(1.0f);
    CHECK_NEAR(h->position.x, 20.0f, 0.01f);
    CHECK_NEAR(h->position.y, 200.0f, 0.01f);

    // borders clamp
    StubResetInput();
    h->position = (Vector2){5, 5};
    StubSetKeyDown(KEY_W, true);
    StubSetKeyDown(KEY_A, true);
    UpdateHero(1.0f);
    CHECK(h->position.x >= 10 && h->position.y >= 10);

    // no input: no movement
    StubResetInput();
    h->position = (Vector2){300, 300};
    UpdateHero(0.5f);
    CHECK(h->position.x == 300 && h->position.y == 300);
}

static void TestUpdateHeroDash(void) {
    ResetForTest();
    Hero* h = &game.hero;
    h->position = (Vector2){200, 200};
    h->lastMovementDirection = (Vector2){1, 0};
    StubPressKey(KEY_Q);
    UpdateHero(1.0f);
    CHECK(h->dashTimer > 0);
    CHECK(h->currentDashCooldown > 0);
    CHECK(h->position.x > 200.0f); // dashed right
    float cd = h->currentDashCooldown;
    StubResetInput();
    UpdateHero(0.1f);
    CHECK(h->currentDashCooldown < cd); // cooldown ticks down
    CHECK(h->dashTimer < 0.2f);
}

static void TestHeroAttack(void) {
    ResetForTest();
    Hero* h = &game.hero;
    h->position = (Vector2){200, 200};
    PlaceEnemyExact(0, ENEMY_BASIC, 200, 240, 100); // 40 < attackRange 65
    float hp0 = game.enemies[0].hp;
    StubSetKeyDown(KEY_SPACE, true);
    UpdateHero(1.0f);
    CHECK(game.enemies[0].hp < hp0);
    CHECK(h->currentCooldown == h->attackCooldown);
    // 35 physical vs armor 10 -> 35*100/110
    CHECK_NEAR(hp0 - game.enemies[0].hp, 35.0f * 100.0f / 110.0f, 1.0f);

    // no enemy in range: no attack
    StubResetInput();
    PlaceEnemyExact(1, ENEMY_BASIC, 200, 400, 100); // out of range
    float hp2 = game.enemies[1].hp;
    h->currentCooldown = 0;
    StubSetKeyDown(KEY_SPACE, true);
    UpdateHero(0.5f);
    CHECK(game.enemies[1].hp == hp2);
}

static void TestHeroAttackKills(void) {
    ResetForTest();
    Hero* h = &game.hero;
    h->position = (Vector2){200, 200};
    PlaceEnemyExact(0, ENEMY_BASIC, 200, 240, 20);
    int gold0 = game.gold;
    StubSetKeyDown(KEY_SPACE, true);
    UpdateHero(1.0f);
    CHECK(!game.enemies[0].active);
    CHECK(game.gold > gold0); // death reward
}

static void TestHeroBurst(void) {
    ResetForTest();
    Hero* h = &game.hero;
    h->position = (Vector2){200, 200};
    PlaceEnemyExact(0, ENEMY_TANK, 200, 260, 600);   // survives, gets weakened
    PlaceEnemyExact(1, ENEMY_BASIC, 200, 300, 100);  // dies to exactly 100 true dmg (hp == burstDamage)
    StubPressKey(KEY_E);
    UpdateHero(1.0f);
    CHECK(game.enemies[0].active);
    bool weakened = false;
    for (int i = 0; i < game.enemies[0].statusCount; i++)
        if (game.enemies[0].status[i].type == STATUS_WEAKEN) weakened = true;
    CHECK(weakened);
    CHECK(!game.enemies[1].active);
    CHECK(h->currentBurstCooldown == h->burstCooldown);
}

static void TestHeroLevelUp(void) {
    ResetForTest();
    Hero* h = &game.hero;
    h->xp = 240;
    GrantHeroXP(60); // 300 -> level 2
    CHECK(h->level == 2);
    CHECK(h->skillPoints == 1);
    CHECK(h->xp == 50);
    CHECK(h->xpToNextLevel == 400);

    GrantHeroXP(1000); // multi-level
    CHECK(h->level > 2);
    CHECK(h->xp < h->xpToNextLevel);

    int lvl = h->level, sp = h->skillPoints;
    LevelUpHero();
    CHECK(h->level == lvl + 1);
    CHECK(h->skillPoints == sp + 1);
}

void TestHero(void) {
    TestApplyHeroSkills();
    TestUpdateHeroMovement();
    TestUpdateHeroDash();
    TestHeroAttack();
    TestHeroAttackKills();
    TestHeroBurst();
    TestHeroLevelUp();
}
