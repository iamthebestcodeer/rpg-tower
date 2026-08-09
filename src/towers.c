#include "game.h"

//----------------------------------------------------------------------------------
// Towers
//----------------------------------------------------------------------------------

void UpdateTowers(float dt) {

    for (int i = 0; i < MAX_TOWERS; i++) {
        if (!game.towers[i].active) continue;
        Tower* t = &game.towers[i];
        t->cooldownTimer -= dt;
        t->visualRecoil = Lerp(t->visualRecoil, 0.0f, 10.0f * dt);

        // Blizzard aura
        if (t->type == TOWER_T4_CRYO_BLIZZARD) {
            int nearby[MAX_ENEMIES];
            int nearbyCount = 0;
            GetEnemiesInRadius(t->position, t->stats.range, nearby, &nearbyCount, MAX_ENEMIES);
            for (int j = 0; j < nearbyCount; j++) {
                Enemy* e = &game.enemies[nearby[j]];
                ApplyStatusEffect(e, STATUS_SLOW, dt * 1.5f, t->stats.fireRate);
                ApplyStatusEffect(e, STATUS_BRITTLE, dt * 1.5f, 1.2f);
                float damage = t->stats.damage * dt;
                float actualDamage = CalculateDamage(damage, t->damageType, e);
                e->hp -= actualDamage;
                if (e->hp <= 0)
                    HandleEnemyDeath(nearby[j], i);
            }
            continue;
        }

        // Validate target
        Enemy* target = GetEnemyFromPair(t->targetIndex, t->targetEnemyId);
        bool hasGlobalRange = (t->stats.range >= SCREEN_WIDTH * 2.0f);
        if (target) {
            if (!hasGlobalRange) {
                float rangeSqr = t->stats.range * t->stats.range;
                float distSqr = Vector2DistanceSqr(t->position, target->position);
                if (distSqr > rangeSqr) target = NULL;
            }
        }
        if (!target) {
            t->targetIndex = -1;
            t->targetEnemyId = -1;
        }

        // Throttled target acquisition
        if (!target) {
            t->targetSearchTimer -= dt;
            if (t->targetSearchTimer <= 0.0f) {
                t->targetSearchTimer = 0.1f; // 10Hz when idle

                int bestTargetIndex = -1;
                float bestMetricPrimary = -1.0f;
                float bestMetricSecondary = FLT_MAX;
                float rangeSqr = hasGlobalRange ? FLT_MAX : (t->stats.range * t->stats.range);

                // Query grid for potential targets
                int candidates[MAX_ENEMIES];
                int candidateCount = 0;
                if (hasGlobalRange) {
                    // If global, just scan all enemies (rare)
                    for (int j = 0; j < MAX_ENEMIES; j++) {
                        if (game.enemies[j].active) {
                            candidates[candidateCount++] = j;
                            if (candidateCount >= MAX_ENEMIES) break;
                        }
                    }
                } else {
                    GetEnemiesInRadius(t->position, t->stats.range, candidates, &candidateCount, MAX_ENEMIES);
                }

                for (int idx = 0; idx < candidateCount; idx++) {
                    int j = candidates[idx];
                    if (!game.enemies[j].active) continue;
                    float distSqr = Vector2DistanceSqr(t->position, game.enemies[j].position);
                    if (!hasGlobalRange && distSqr > rangeSqr) continue;

                    bool better = false;
                    switch (t->targetingMode) {
                        case TARGET_FIRST: {
                            float currentPrimary = (float)game.enemies[j].waypointIndex;
                            float currentSecondary = FLT_MAX;
                            if (game.enemies[j].waypointIndex < game.map.waypointCount)
                                currentSecondary = Vector2DistanceSqr(game.enemies[j].position, game.map.waypoints[game.enemies[j].waypointIndex]);
                            if (bestTargetIndex == -1) better = true;
                            else if (currentPrimary > bestMetricPrimary) better = true;
                            else if (currentPrimary == bestMetricPrimary && currentSecondary < bestMetricSecondary) better = true;
                            if (better) { bestMetricPrimary = currentPrimary; bestMetricSecondary = currentSecondary; }
                        } break;
                        case TARGET_CLOSEST: {
                            if (bestTargetIndex == -1 || distSqr < bestMetricSecondary) {
                                better = true; bestMetricSecondary = distSqr;
                            }
                        } break;
                        case TARGET_STRONGEST: {
                            float currentPrimary = game.enemies[j].hp;
                            if (bestTargetIndex == -1 || currentPrimary > bestMetricPrimary) {
                                better = true; bestMetricPrimary = currentPrimary;
                            }
                        } break;
                        case TARGET_WEAKEST: {
                            float currentSecondary = game.enemies[j].hp;
                            if (bestTargetIndex == -1 || currentSecondary < bestMetricSecondary) {
                                better = true; bestMetricSecondary = currentSecondary;
                            }
                        } break;
                    }
                    if (better) bestTargetIndex = j;
                }

                if (bestTargetIndex != -1) {
                    t->targetIndex = bestTargetIndex;
                    t->targetEnemyId = game.enemies[bestTargetIndex].id;
                    target = &game.enemies[bestTargetIndex];
                }
            }
        } else {
            // When we have a target, we can search immediately if lost
            t->targetSearchTimer = 0.0f;
        }

        // Rotation / firing
        if (target) {
            Vector2 direction = Vector2Subtract(target->position, t->position);
            t->desiredRotation = atan2f(direction.y, direction.x) * RAD2DEG;

            float rotationSpeed = 18.0f;
            if (t->type == TOWER_T4_PULSE_SNIPER || t->type == TOWER_CANNON || t->type == TOWER_T4_CANNON_MORTAR) rotationSpeed = 8.0f;
            float rotAlpha = fminf(rotationSpeed * dt, 1.0f);
            t->rotation = LerpAngle(t->rotation, t->desiredRotation, rotAlpha);

            float angleDiff = GetAngleDifference(t->rotation, t->desiredRotation);
            float aimTolerance = GetAimToleranceDegrees(t->type);

            if (t->cooldownTimer <= 0 && (angleDiff <= aimTolerance || hasGlobalRange)) {
                if (t->type == TOWER_CRYO || t->type == TOWER_T4_CRYO_FREEZER) {
                    if (t->type == TOWER_CRYO) {
                        ApplyStatusEffect(target, STATUS_SLOW, dt * 1.5f, 0.6f);
                        ApplyStatusEffect(target, STATUS_BRITTLE, dt * 1.5f, 1.15f);
                    } else {
                        ApplyStatusEffect(target, STATUS_STUN, t->stats.fireRate, 1.0f);
                        t->cooldownTimer = (t->stats.fireRate > 0) ? (1.0f / t->stats.fireRate) : 1.0f;
                    }
                    float damage = t->stats.damage * dt;
                    float actualDamage = CalculateDamage(damage, t->damageType, target);
                    target->hp -= actualDamage;
                    if (target->hp <= 0) {
                        int targetIndex = (int)(target - game.enemies);
                        HandleEnemyDeath(targetIndex, i);
                    }
                } else {
                    FireProjectile(t, target);
                    if (t->type == TOWER_CANNON || t->type == TOWER_T4_CANNON_MORTAR || t->type == TOWER_T4_CANNON_VULCAN)
                        t->visualRecoil = 8.0f;
                    else
                        t->visualRecoil = 4.0f;
                    t->cooldownTimer = (t->stats.fireRate > 0) ? (1.0f / t->stats.fireRate) : 1.0f;
                }
            }
        } else {
            t->desiredRotation = 0.0f;
            if (t->stats.range < SCREEN_WIDTH * 2) {
                float rotAlphaIdle = fminf(5.0f * dt, 1.0f);
                t->rotation = LerpAngle(t->rotation, t->desiredRotation, rotAlphaIdle);
            }
        }
    }
}

//----------------------------------------------------------------------------------
// Tower Placement & Upgrade
//----------------------------------------------------------------------------------

bool PlaceTower(int x, int y, TowerType type) {
    if (!IsTileBuildable(x, y)) {
        AddFloatingText(TileToWorldCenter(x,y), "Cannot build here!", COLOR_DANGER, false);
        return false;
    }
    int cost = GetTowerCost(type);
    if (game.gold < cost) {
        AddFloatingText(TileToWorldCenter(x,y), "Not enough gold!", COLOR_DANGER, false);
        return false;
    }

    int index = -1;
    for (int i = 0; i < MAX_TOWERS; i++) {
        if (!game.towers[i].active) { index = i; break; }
    }
    if (index == -1) return false;

    game.gold -= cost;
    Tower* t = &game.towers[index];
    memset(t, 0, sizeof(Tower));
    t->active = true;
    t->type = type;
    t->position = TileToWorldCenter(x, y);
    Vector2 tp = WorldToTile(t->position);
    int tx = (int)tp.x, ty = (int)tp.y;
    if (tx >= 0 && tx < MAP_WIDTH && ty >= 0 && ty < MAP_HEIGHT)
        game.occupied[ty][tx] = true;
    t->targetIndex = -1;
    t->targetEnemyId = -1;
    t->stats.level = 1;
    t->targetingMode = TARGET_FIRST;
    t->totalCost = cost;
    ConfigureTowerStats(t);
    SpawnParticles(t->position, 40, COLOR_ENERGY, Fade(COLOR_ENERGY, 0.0f), 80.0f, 8.0f, 2.0f, false);
    return true;
}

void SellTower(int towerIndex) {
    if (towerIndex < 0 || towerIndex >= MAX_TOWERS || !game.towers[towerIndex].active) return;
    Tower* t = &game.towers[towerIndex];
    int sellValue = (int)(t->totalCost * 0.6f);
    game.gold += sellValue;

    Vector2 tp = WorldToTile(t->position);
    int tx = (int)tp.x, ty = (int)tp.y;
    if (tx >= 0 && tx < MAP_WIDTH && ty >= 0 && ty < MAP_HEIGHT)
        game.occupied[ty][tx] = false;

    AddFloatingTextFmt(t->position, COLOR_GOLD, false, "SOLD +%dG", sellValue);
    SpawnParticles(t->position, 30, GRAY, Fade(GRAY, 0.0f), 80.0f, 8.0f, 2.0f, true);
    t->active = false;
    game.selectedTowerIndex = -1;
}

bool UpgradeTower(Tower* t, TowerType newType) {
    if (!t || !t->active) return false;
    bool valid = false;
    switch (t->type) {
        case TOWER_PULSE: if (newType == TOWER_T4_PULSE_REPEATER || newType == TOWER_T4_PULSE_SNIPER) valid = true; break;
        case TOWER_CANNON: if (newType == TOWER_T4_CANNON_MORTAR || newType == TOWER_T4_CANNON_VULCAN) valid = true; break;
        case TOWER_CRYO: if (newType == TOWER_T4_CRYO_BLIZZARD || newType == TOWER_T4_CRYO_FREEZER) valid = true; break;
        case TOWER_TESLA: if (newType == TOWER_T4_TESLA_CHAIN || newType == TOWER_T4_TESLA_STORM) valid = true; break;
        default: break;
    }
    if (valid) {
        t->type = newType;
        ConfigureTowerStats(t);
        SpawnParticles(t->position, 100, COLOR_AETHER_RES, Fade(YELLOW, 0.0f), 150.0f, 10.0f, 3.0f, false);
        return true;
    }
    return false;
}

void ConfigureTowerStats(Tower* tower) {
    TowerStats* s = &tower->stats;
    if (tower->type >= TOWER_PULSE && tower->type <= TOWER_TESLA && s->level < TOWER_BASE_MAX_LEVEL) {
        int effectiveLevel = (s->level > 0) ? s->level : 1;
        s->xpToNextLevel = (int)(150 * pow(1.9, effectiveLevel - 1));
    } else {
        s->xpToNextLevel = 99999;
    }

    switch (tower->type) {
        case TOWER_PULSE: s->damage = 20; s->range = 150.0f; s->fireRate = 2.5f; tower->damageType = DMG_ENERGY; break;
        case TOWER_CANNON: s->damage = 80; s->range = 130.0f; s->fireRate = 0.5f; tower->damageType = DMG_PHYSICAL; break;
        case TOWER_CRYO: s->damage = 20; s->range = 140.0f; s->fireRate = 0.0f; tower->damageType = DMG_ENERGY; break;
        case TOWER_TESLA: s->damage = 50; s->range = 180.0f; s->fireRate = 1.1f; tower->damageType = DMG_ENERGY; break;
        case TOWER_T4_PULSE_REPEATER: s->damage = 25; s->range = 160.0f; s->fireRate = 8.0f; tower->damageType = DMG_ENERGY; break;
        case TOWER_T4_PULSE_SNIPER: s->damage = 300; s->range = 400.0f; s->fireRate = 0.8f; tower->damageType = DMG_ENERGY; break;
        case TOWER_T4_CANNON_MORTAR: s->damage = 500; s->range = 9999.0f; s->fireRate = 0.3f; tower->damageType = DMG_PHYSICAL; break;
        case TOWER_T4_CANNON_VULCAN: s->damage = 30; s->range = 140.0f; s->fireRate = 10.0f; tower->damageType = DMG_PHYSICAL; break;
        case TOWER_T4_CRYO_BLIZZARD: s->damage = 10; s->range = 120.0f; s->fireRate = 0.75f; tower->damageType = DMG_ENERGY; break;
        case TOWER_T4_CRYO_FREEZER: s->damage = 50; s->range = 150.0f; s->fireRate = 1.5f; tower->damageType = DMG_ENERGY; break;
        case TOWER_T4_TESLA_CHAIN: s->damage = 80; s->range = 200.0f; s->fireRate = 1.5f; tower->damageType = DMG_ENERGY; break;
        case TOWER_T4_TESLA_STORM: s->damage = 200; s->range = 220.0f; s->fireRate = 0.9f; tower->damageType = DMG_ENERGY; break;
        default: return;
    }

    int scalingLevel = (s->level > 0) ? s->level : 1;
    float damageMult = 1.0f + (scalingLevel - 1) * 0.35f;
    float rangeMult = 1.0f + (scalingLevel - 1) * 0.10f;
    float rateMult = 1.0f + (scalingLevel - 1) * 0.15f;

    s->damage *= damageMult;
    s->range *= rangeMult;
    if (tower->type != TOWER_CRYO && tower->type != TOWER_T4_CRYO_BLIZZARD && tower->type != TOWER_T4_CRYO_FREEZER)
        s->fireRate *= rateMult;
}

//----------------------------------------------------------------------------------
// Game Logic Helpers
//----------------------------------------------------------------------------------

void GrantXP(int towerIndex, int xp) {
    if (towerIndex < 0 || towerIndex >= MAX_TOWERS || !game.towers[towerIndex].active) return;
    Tower* t = &game.towers[towerIndex];
    if (t->type < TOWER_PULSE || t->type > TOWER_TESLA) return;

    if (game.hero.skills[SKILL_LEADERSHIP] > 0) {
        if (Vector2Distance(t->position, game.hero.position) <= 150.0f) {
            xp += (int)(xp * game.hero.skills[SKILL_LEADERSHIP] * 0.15f);
        }
    }
    t->stats.xp += xp;
    AddFloatingTextFmt(t->position, COLOR_XP, false, "+%d XP", xp);

    while (t->stats.xp >= t->stats.xpToNextLevel && t->stats.level < TOWER_BASE_MAX_LEVEL) {
        LevelUpTower(towerIndex);
    }
}

void LevelUpTower(int towerIndex) {
    Tower* t = &game.towers[towerIndex];
    t->stats.xp -= t->stats.xpToNextLevel;
    t->stats.level++;
    ConfigureTowerStats(t);
    AddFloatingText(t->position, "LEVEL UP!", YELLOW, true);
    SpawnParticles(t->position, 60, YELLOW, Fade(YELLOW, 0.0f), 120.0f, 10.0f, 3.0f, false);
}
