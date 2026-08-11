// Tests for projectiles.c: firing, flight, impacts, chain lightning, effects.
#include "test_util.h"

static Tower* TowerAt(int i) { return &game.towers[i]; }

static void TestFireProjectileTypes(void) {
    ResetForTest();
    int e = SpawnEnemyAt(ENEMY_BASIC, 500, 500);
    Enemy* target = &game.enemies[e];

    Tower pulse = {0};
    pulse.type = TOWER_PULSE; pulse.stats.damage = 20; pulse.stats.level = 1;
    pulse.damageType = DMG_ENERGY; pulse.position = (Vector2){100, 100};
    FireProjectile(&pulse, target);
    Projectile* p = &game.projectiles[0];
    CHECK(p->active);
    CHECK_NEAR(p->speed, 650.0f, 0.01f);
    CHECK(p->targetEnemyId == target->id);
    CHECK(p->lifetime == 10.0f);

    Tower cannon = {0};
    cannon.type = TOWER_CANNON; cannon.stats.damage = 80; cannon.stats.level = 2;
    cannon.damageType = DMG_PHYSICAL; cannon.position = (Vector2){100, 100};
    FireProjectile(&cannon, target);
    Projectile* c = &game.projectiles[1];
    CHECK_NEAR(c->speed, 350.0f, 0.01f);
    CHECK_NEAR(c->aoeRadius, 50.0f + 2 * 10.0f, 0.01f);
    CHECK(c->applyStatus == STATUS_BURN);
    CHECK_NEAR(c->statusIntensity, 10.0f + 2 * 5.0f, 0.01f);

    Tower mortar = {0};
    mortar.type = TOWER_T4_CANNON_MORTAR; mortar.stats.damage = 500; mortar.stats.level = 1;
    mortar.damageType = DMG_PHYSICAL; mortar.position = (Vector2){100, 100};
    FireProjectile(&mortar, target);
    Projectile* m = &game.projectiles[2];
    CHECK_NEAR(m->speed, 250.0f, 0.01f);
    CHECK_NEAR(m->aoeRadius, 150.0f, 0.01f);
    CHECK_NEAR(m->lifetime, 6.0f, 0.01f);

    Tower tesla = {0};
    tesla.type = TOWER_TESLA; tesla.stats.damage = 50; tesla.stats.level = 1;
    tesla.damageType = DMG_ENERGY; tesla.position = (Vector2){100, 100};
    FireProjectile(&tesla, target);
    Projectile* ts = &game.projectiles[3];
    CHECK(ts->applyStatus == STATUS_STUN);
    CHECK_NEAR(ts->statusDuration, 0.6f, 0.01f);
    CHECK_NEAR(ts->statusIntensity, 20.0f + 7.0f, 0.01f);

    Tower chain = {0};
    chain.type = TOWER_T4_TESLA_CHAIN; chain.stats.damage = 80; chain.stats.level = 1;
    chain.damageType = DMG_ENERGY; chain.position = (Vector2){100, 100};
    FireProjectile(&chain, target);
    Projectile* ch = &game.projectiles[4];
    CHECK(ch->chainCount == 4); // 3 + level
    CHECK_NEAR(ch->speed, 900.0f, 0.01f);

    Tower vulcan = {0};
    vulcan.type = TOWER_T4_CANNON_VULCAN; vulcan.stats.damage = 30; vulcan.stats.level = 1;
    vulcan.damageType = DMG_PHYSICAL; vulcan.position = (Vector2){100, 100};
    FireProjectile(&vulcan, target);
    Projectile* v = &game.projectiles[5];
    CHECK_NEAR(v->speed, 700.0f, 0.01f);

    Tower sniper = {0};
    sniper.type = TOWER_T4_PULSE_SNIPER; sniper.stats.damage = 300; sniper.stats.level = 1;
    sniper.damageType = DMG_ENERGY; sniper.position = (Vector2){100, 100};
    FireProjectile(&sniper, target);
    Projectile* sn = &game.projectiles[6];
    CHECK_NEAR(sn->speed, 1200.0f, 0.01f);

    // invalid source type -> projectile deactivated, no slot consumed
    Tower weird = {0};
    weird.type = TOWER_NONE; weird.stats.damage = 10; weird.damageType = DMG_ENERGY;
    weird.position = (Vector2){100, 100};
    FireProjectile(&weird, target);
    CHECK(CountActiveProjectiles() == 7);
}

static void TestFireProjectilePoolFull(void) {
    ResetForTest();
    for (int i = 0; i < MAX_PROJECTILES; i++) game.projectiles[i].active = true;
    Tower t = {0};
    t.type = TOWER_PULSE; t.stats.damage = 20; t.stats.level = 1; t.damageType = DMG_ENERGY;
    int e = SpawnEnemyAt(ENEMY_BASIC, 500, 500);
    FireProjectile(&t, &game.enemies[e]);
    CHECK(CountActiveProjectiles() == MAX_PROJECTILES);
}

static void TestProjectileImpact(void) {
    ResetForTest();
    PlaceTower(5, 5, TOWER_PULSE);
    Tower* t = TowerAt(0);
    int e = SpawnEnemyAt(ENEMY_BASIC, 220, 240); // 20 from tower
    t->targetIndex = e;
    t->targetEnemyId = game.enemies[e].id;
    t->rotation = 90.0f;
    t->desiredRotation = 90.0f;
    UpdateTowers(0.1f);
    Projectile* p = &game.projectiles[0];
    CHECK(p->active);
    UpdateProjectiles(0.1f); // travel 65 > dist 20 -> impact
    CHECK(!p->active);
    CHECK(game.enemies[e].hp < game.enemies[e].maxHp);
}

static void TestProjectileMoveAndExpire(void) {
    ResetForTest();
    int e = SpawnEnemyAt(ENEMY_BASIC, 600, 600);
    Tower pulse = {0};
    pulse.type = TOWER_PULSE; pulse.stats.damage = 20; pulse.stats.level = 1;
    pulse.damageType = DMG_ENERGY; pulse.position = (Vector2){100, 100};
    FireProjectile(&pulse, &game.enemies[e]);
    Projectile* p = &game.projectiles[0];
    Vector2 start = p->position;
    game.enemies[e].position = (Vector2){2000, 2000}; // far away
    UpdateProjectiles(0.1f);
    CHECK(p->active);
    CHECK(DistSqr(p->position, start) > 1.0f);

    // lifetime expiry (mortars)
    Tower mortar = {0};
    mortar.type = TOWER_T4_CANNON_MORTAR; mortar.stats.damage = 500; mortar.stats.level = 1;
    mortar.damageType = DMG_PHYSICAL; mortar.position = (Vector2){100, 100};
    FireProjectile(&mortar, &game.enemies[e]);
    Projectile* m = &game.projectiles[1];
    m->lifetime = 0.01f;
    UpdateProjectiles(0.1f);
    CHECK(!m->active);

    // target lost mid-flight -> projectile deactivated
    Tower tesla = {0};
    tesla.type = TOWER_TESLA; tesla.stats.damage = 50; tesla.stats.level = 1;
    tesla.damageType = DMG_ENERGY; tesla.position = (Vector2){100, 100};
    FireProjectile(&tesla, &game.enemies[e]);
    Projectile* tp = &game.projectiles[2];
    game.enemies[e].active = false;
    UpdateProjectiles(0.1f);
    CHECK(!tp->active);
}

static void TestImpactAoE(void) {
    ResetForTest();
    // exact placement so distances are deterministic (no path snapping)
    PlaceEnemyExact(0, ENEMY_BASIC, 400, 400, 100);
    PlaceEnemyExact(1, ENEMY_BASIC, 430, 400, 100);
    PlaceEnemyExact(2, ENEMY_BASIC, 700, 400, 100); // outside aoe
    Projectile p = {0};
    p.active = true;
    p.position = (Vector2){400, 400};
    p.damage = 80; p.damageType = DMG_PHYSICAL;
    p.sourceType = TOWER_CANNON;
    p.aoeRadius = 100.0f;
    p.lifetime = 10;
    float hpA = game.enemies[0].hp, hpB = game.enemies[1].hp, hpC = game.enemies[2].hp;
    // NOTE: the crit roll happens after particle spawning, so it draws from the
    // seeded LCG (1/101 chance); assertions here are damage-bounded, so a
    // random crit can't flip them.
    HandleProjectileImpact(&p, &game.enemies[0]);
    CHECK(game.enemies[0].hp < hpA);
    CHECK(game.enemies[1].hp < hpB);
    CHECK(game.enemies[2].hp == hpC); // untouched
}

static void TestChainLightning(void) {
    ResetForTest();
    game.gold = 5000; game.aether = 200;
    PlaceTower(9, 5, TOWER_TESLA); // (380,220)
    Tower* t = TowerAt(0);
    UpgradeTower(t, TOWER_T4_TESLA_CHAIN);
    // exact placement; enemies within 150 of each other and the tower
    PlaceEnemyExact(0, ENEMY_BASIC, 380, 260, 100);
    PlaceEnemyExact(1, ENEMY_BASIC, 420, 260, 100);
    PlaceEnemyExact(2, ENEMY_BASIC, 460, 260, 100);
    t->targetIndex = 0;
    t->targetEnemyId = game.enemies[0].id;
    t->rotation = 90.0f;
    t->desiredRotation = 90.0f;
    UpdateTowers(0.1f);
    CHECK(CountActiveProjectiles() == 1);
    float hpA = game.enemies[0].hp, hpB = game.enemies[1].hp, hpC = game.enemies[2].hp;
    UpdateProjectiles(0.1f); // hit 0, chain to 1 (and on to 2 same frame)
    CHECK(game.enemies[0].hp < hpA);
    UpdateProjectiles(0.1f);
    UpdateProjectiles(0.1f);
    CHECK(game.enemies[1].hp < hpB);
    CHECK(game.enemies[2].hp < hpC);
}

static void TestApplyDamageAndEffects(void) {
    ResetForTest();
    int e = SpawnEnemyAt(ENEMY_BASIC, 100, 100);
    Enemy* en = &game.enemies[e];
    Projectile p = {0};
    p.damage = 50; p.damageType = DMG_PHYSICAL; p.sourceType = TOWER_CANNON;
    p.applyStatus = STATUS_BURN; p.statusDuration = 5.0f; p.statusIntensity = 15.0f;
    float hp0 = en->hp;
    ApplyDamageAndEffects(&p, en, true); // critical
    CHECK_NEAR(hp0 - en->hp, 100.0f * 100.0f / 110.0f, 1.0f);
    CHECK(en->statusCount == 2); // melted armor + burn

    // stun: intensity 0 -> roll is `GetRandomValue(0,100) >= 0`, always true,
    // so the stun is never applied (logic-guaranteed, no RNG dependence)
    int e2 = SpawnEnemyAt(ENEMY_TANK, 300, 300);
    Projectile s = {0};
    s.damage = 10; s.damageType = DMG_ENERGY; s.sourceType = TOWER_TESLA;
    s.applyStatus = STATUS_STUN; s.statusDuration = 1; s.statusIntensity = 0;
    ApplyDamageAndEffects(&s, &game.enemies[e2], false);
    CHECK(game.enemies[e2].statusCount == 0);

    // stun: intensity 101 -> roll `>= 101` is never true, always applied
    // (also logic-guaranteed)
    Projectile s2 = {0};
    s2.damage = 10; s2.damageType = DMG_ENERGY; s2.sourceType = TOWER_TESLA;
    s2.applyStatus = STATUS_STUN; s2.statusDuration = 1; s2.statusIntensity = 101;
    ApplyDamageAndEffects(&s2, &game.enemies[e2], false);
    bool stunned = false;
    for (int i = 0; i < game.enemies[e2].statusCount; i++)
        if (game.enemies[e2].status[i].type == STATUS_STUN) stunned = true;
    CHECK(stunned);

    // killing blow grants rewards
    int gold0 = game.gold;
    int e3 = SpawnEnemyAt(ENEMY_BASIC, 400, 400);
    game.enemies[e3].hp = 5;
    Projectile k = {0};
    k.damage = 100; k.damageType = DMG_TRUE; k.sourceType = TOWER_PULSE; k.sourceTowerIndex = -1;
    ApplyDamageAndEffects(&k, &game.enemies[e3], false);
    CHECK(!game.enemies[e3].active);
    CHECK(game.gold > gold0);
}

void TestProjectiles(void) {
    TestFireProjectileTypes();
    TestFireProjectilePoolFull();
    TestProjectileImpact();
    TestProjectileMoveAndExpire();
    TestImpactAoE();
    TestChainLightning();
    TestApplyDamageAndEffects();
}
