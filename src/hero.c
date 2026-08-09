#include "game.h"

//----------------------------------------------------------------------------------
// Hero
//----------------------------------------------------------------------------------

void ApplyHeroSkills(void) {
    Hero* h = &game.hero;
    float baseSpeed = 180.0f + (h->level - 1) * 5.0f;
    int baseAttack = 35 + (h->level - 1) * 10;
    float baseDashCD = 6.0f * powf(0.95f, (float)(h->level - 1));
    float baseBurstDmg = 100.0f + (h->level - 1) * 25.0f;
    float baseBurstRange = 120.0f + (h->level - 1) * 5.0f;

    h->attackDamage = baseAttack + h->skills[SKILL_VIGOR] * 15;
    h->speed = baseSpeed * (1.0f + h->skills[SKILL_AGILITY] * 0.1f);
    h->dashCooldown = baseDashCD * powf(0.9f, (float)h->skills[SKILL_AGILITY]);
    h->burstDamage = baseBurstDmg * (1.0f + h->skills[SKILL_BURST_MASTERY] * 0.2f);
    h->burstRange = baseBurstRange * (1.0f + h->skills[SKILL_BURST_MASTERY] * 0.1f);
}

void UpdateHero(float dt) {
    Hero* h = &game.hero;

    Vector2 movementInput = {0, 0};
    if (IsKeyDown(KEY_W)) movementInput.y -= 1;
    if (IsKeyDown(KEY_S)) movementInput.y += 1;
    if (IsKeyDown(KEY_A)) movementInput.x -= 1;
    if (IsKeyDown(KEY_D)) movementInput.x += 1;

    if (Vector2Length(movementInput) > 0) {
        movementInput = Vector2Normalize(movementInput);
        h->lastMovementDirection = movementInput;
    }

    h->currentDashCooldown -= dt;
    h->dashTimer -= dt;

    float currentSpeed = h->speed;
    Vector2 moveVector = {0, 0};

    if (h->dashTimer > 0) {
        currentSpeed *= 4.5f;
        moveVector = h->dashDirection;
    } else {
        moveVector = movementInput;
        if (IsKeyPressed(KEY_Q) && h->currentDashCooldown <= 0) {
            h->dashTimer = 0.2f;
            h->currentDashCooldown = h->dashCooldown;
            if (Vector2Length(movementInput) > 0)
                h->dashDirection = movementInput;
            else
                h->dashDirection = h->lastMovementDirection;
            moveVector = h->dashDirection;
            currentSpeed *= 4.5f;
            SpawnParticles(h->position, 30, COLOR_AETHER_RES, Fade(COLOR_AETHER_RES, 0.0f), 80.0f, 8.0f, 2.0f, false);
        }
    }

    if (Vector2Length(moveVector) > 0) {
        h->position = Vector2Add(h->position, Vector2Scale(moveVector, currentSpeed * dt));
    }
    h->position.x = Clamp(h->position.x, 10, GAME_AREA_WIDTH - 10);
    h->position.y = Clamp(h->position.y, 10, SCREEN_HEIGHT - 10);

    h->currentCooldown -= dt;
    if (IsKeyDown(KEY_SPACE) && h->currentCooldown <= 0) {
        HeroAttack();
        h->currentCooldown = h->attackCooldown;
    }

    h->currentBurstCooldown -= dt;
    if (IsKeyPressed(KEY_E) && h->currentBurstCooldown <= 0) {
        AddFloatingText(h->position, "AETHER BURST!", COLOR_ENERGY, true);
        ScreenShake(5.0f, 0.3f);
        SpawnParticles(h->position, 200, COLOR_ENERGY, Fade(WHITE, 0.5f), 150.0f, 10.0f, 2.0f, false);
        SpawnParticles(h->position, 50, COLOR_AETHER_RES, Fade(COLOR_AETHER_RES, 0.0f), 50.0f, 15.0f, 5.0f, false);

        // Use spatial grid for burst AoE
        int nearby[MAX_ENEMIES];
        int nearbyCount = 0;
        GetEnemiesInRadius(h->position, h->burstRange, nearby, &nearbyCount, MAX_ENEMIES);

        for (int i = 0; i < nearbyCount; i++) {
            Enemy* e = &game.enemies[nearby[i]];
            float actualDamage = CalculateDamage(h->burstDamage, DMG_TRUE, e);
            e->hp -= actualDamage;
            AddFloatingTextFmt(e->position, COLOR_ENERGY, true, "%.0f", actualDamage);
            ApplyStatusEffect(e, STATUS_WEAKEN, 6.0f, 1.25f);
            if (e->hp <= 0)
                HandleEnemyDeath(nearby[i], -2);
        }
        h->currentBurstCooldown = h->burstCooldown;
    }
}

void HeroAttack(void) {
    Hero* h = &game.hero;
    int nearby[MAX_ENEMIES];
    int nearbyCount = 0;
    GetEnemiesInRadius(h->position, h->attackRange, nearby, &nearbyCount, MAX_ENEMIES);

    for (int i = 0; i < nearbyCount; i++) {
        Enemy* e = &game.enemies[nearby[i]];
        float actualDamage = CalculateDamage((float)h->attackDamage, DMG_PHYSICAL, e);
        e->hp -= actualDamage;
        AddFloatingTextFmt(e->position, COLOR_AETHER_RES, false, "%.0f", actualDamage);
        SpawnParticles(e->position, 8, COLOR_AETHER_RES, Fade(WHITE, 0.0f), 100.0f, 5.0f, 1.0f, false);
        if (e->hp <= 0)
            HandleEnemyDeath(nearby[i], -2);
    }
}

void GrantHeroXP(int xp) {
    Hero* h = &game.hero;
    h->xp += xp;
    while (h->xp >= h->xpToNextLevel)
        LevelUpHero();
}

void LevelUpHero(void) {
    Hero* h = &game.hero;
    h->xp -= h->xpToNextLevel;
    h->level++;
    h->xpToNextLevel = (int)(h->xpToNextLevel * 1.6f);
    h->skillPoints++;
    ApplyHeroSkills();
    AddFloatingText(h->position, "HERO LEVEL UP!", COLOR_AETHER_RES, true);
    SpawnParticles(h->position, 70, COLOR_AETHER_RES, Fade(COLOR_AETHER_RES, 0.0f), 140.0f, 12.0f, 4.0f, false);
}
