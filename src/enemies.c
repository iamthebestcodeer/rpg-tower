#include "game.h"

//----------------------------------------------------------------------------------
// Enemies
//----------------------------------------------------------------------------------

void RebuildEnemyGrid(void) {
    memset(game.enemyGrid, 0, sizeof(game.enemyGrid));
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!game.enemies[i].active) continue;
        int cx = (int)(game.enemies[i].position.x / TILE_SIZE);
        int cy = (int)(game.enemies[i].position.y / TILE_SIZE);
        if (cx < 0 || cx >= GRID_COLS || cy < 0 || cy >= GRID_ROWS) continue;
        GridCell* cell = &game.enemyGrid[cy][cx];
        if (cell->count < MAX_ENEMIES_PER_CELL)
            cell->indices[cell->count++] = i;
    }
}

void GetEnemiesInRadius(Vector2 center, float radius, int* outIndices, int* outCount, int maxOut) {
    *outCount = 0;
    int cx = (int)(center.x / TILE_SIZE);
    int cy = (int)(center.y / TILE_SIZE);
    int radCells = (int)ceilf(radius / TILE_SIZE);
    float radiusSqr = radius * radius;

    for (int dy = -radCells; dy <= radCells; dy++) {
        for (int dx = -radCells; dx <= radCells; dx++) {
            int nx = cx + dx, ny = cy + dy;
            if (nx < 0 || nx >= GRID_COLS || ny < 0 || ny >= GRID_ROWS) continue;
            GridCell* cell = &game.enemyGrid[ny][nx];
            for (int k = 0; k < cell->count; k++) {
                int idx = cell->indices[k];
                if (!game.enemies[idx].active) continue;
                float distSqr = Vector2DistanceSqr(center, game.enemies[idx].position);
                if (distSqr <= radiusSqr) {
                    outIndices[(*outCount)++] = idx;
                    if (*outCount >= maxOut) return;
                }
            }
        }
    }
}

void UpdateEnemies(float dt) {
    // Grid is rebuilt once per frame in UpdateTowers; but we also need it for enemies themselves.
    RebuildEnemyGrid(); // safe to call multiple times; but we call it once at start of UpdateTowers. To avoid double rebuild, we call here and let towers use it too (they rebuild anyway). Let's centralize: call in UpdatePlaying before both.

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!game.enemies[i].active) continue;
        Enemy* e = &game.enemies[i];

        ProcessStatusEffects(e, dt);

        // Healer aura using grid
        if (e->type == ENEMY_HEALER) {
            e->abilityTimer -= dt;
            if (e->abilityTimer <= 0) {
                e->abilityTimer = 2.0f;
                float healAmount = 50.0f + game.currentWave * 5.0f;
                float healRange = 100.0f;

                int nearby[MAX_ENEMIES];
                int nearbyCount = 0;
                GetEnemiesInRadius(e->position, healRange, nearby, &nearbyCount, MAX_ENEMIES);
                for (int k = 0; k < nearbyCount; k++) {
                    int j = nearby[k];
                    if (i == j) continue;
                    if (game.enemies[j].hp < game.enemies[j].maxHp) {
                        game.enemies[j].hp += healAmount;
                        if (game.enemies[j].hp > game.enemies[j].maxHp)
                            game.enemies[j].hp = game.enemies[j].maxHp;
                        AddFloatingTextFmt(game.enemies[j].position, GREEN, false, "+%.0f HP", healAmount);
                    }
                }
                SpawnParticles(e->position, 10, GREEN, Fade(GREEN, 0.0f), 50.0f, 5.0f, 1.0f, false);
            }
        }

        if (e->hp <= 0) {
            HandleEnemyDeath(i, -1);
            continue;
        }

        // Movement (same logic, no change)
        if (e->speed > 0) {
            float moveDistRemaining = e->speed * dt;
            while (moveDistRemaining > 0.0f && e->waypointIndex < game.map.waypointCount) {
                Vector2 target = game.map.waypoints[e->waypointIndex];
                Vector2 direction = Vector2Subtract(target, e->position);
                float distanceToTarget = Vector2Length(direction);
                if (distanceToTarget < 0.001f) {
                    e->position = target;
                    e->waypointIndex++;
                    continue;
                }
                if (distanceToTarget <= moveDistRemaining) {
                    e->position = target;
                    moveDistRemaining -= distanceToTarget;
                    e->waypointIndex++;
                } else {
                    direction = Vector2Normalize(direction);
                    e->position = Vector2Add(e->position, Vector2Scale(direction, moveDistRemaining));
                    moveDistRemaining = 0.0f;
                }
            }
        }

        if (e->waypointIndex >= game.map.waypointCount) {
            game.lives--;
            e->active = false;
            SpawnParticles(e->position, 50, COLOR_DANGER, COLOR_DANGER, 100.0f, 10.0f, 2.0f, false);
            ScreenShake(5.0f, 0.3f);
        }
    }
}

void SpawnEnemy(EnemyType type, Vector2 position) {
    int index = -1;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!game.enemies[i].active) { index = i; break; }
    }
    if (index == -1) return;

    Enemy* e = &game.enemies[index];
    memset(e, 0, sizeof(Enemy));
    e->active = true;
    e->id = game.enemyIdCounter++;
    e->type = type;
    e->position = position;

    if (Vector2DistanceSqr(position, game.map.waypoints[0]) < 1.0f) {
        e->waypointIndex = 1;
    } else {
        float minDistSqr = 1e10f;
        int bestIndex = 1;
        for (int i = 1; i < game.map.waypointCount; i++) {
            Vector2 segStart = game.map.waypoints[i-1];
            Vector2 segEnd = game.map.waypoints[i];
            Vector2 closest = ClosestPointOnSegment(position, segStart, segEnd);
            float distSqr = Vector2DistanceSqr(position, closest);
            if (distSqr < minDistSqr) {
                minDistSqr = distSqr;
                bestIndex = i;
            }
        }
        e->waypointIndex = bestIndex;
        // Snap minion to path
        Vector2 segStart = game.map.waypoints[bestIndex-1];
        Vector2 segEnd = game.map.waypoints[bestIndex];
        e->position = ClosestPointOnSegment(position, segStart, segEnd);
    }

    float hpMult = 1.0f + (game.currentWave - 1) * 0.18f + powf((float)game.currentWave, 1.1f) * 0.01f;
    float armorMult = 1.0f + (game.currentWave - 1) * 0.1f;

    switch (type) {
        case ENEMY_BASIC:
            e->maxHp = 100.0f * hpMult;
            e->baseSpeed = 80.0f;
            e->armor = 10 * armorMult;
            e->energyResist = 10 * armorMult;
            e->goldValue = 10;
            e->aetherValue = (GetRandomValue(0, 100) < 5) ? 1 : 0;
            e->xpValue = 20;
            break;
        case ENEMY_FAST:
            e->maxHp = 75.0f * hpMult;
            e->baseSpeed = 170.0f;
            e->armor = 5 * armorMult;
            e->energyResist = 15 * armorMult;
            e->goldValue = 15;
            e->aetherValue = (GetRandomValue(0, 100) < 8) ? 1 : 0;
            e->xpValue = 25;
            break;
        case ENEMY_TANK:
            e->maxHp = 600.0f * hpMult;
            e->baseSpeed = 50.0f;
            e->armor = 70 * armorMult;
            e->energyResist = 20 * armorMult;
            e->goldValue = 30;
            e->aetherValue = (GetRandomValue(0, 100) < 15) ? 2 : 0;
            e->xpValue = 50;
            break;
        case ENEMY_ETHEREAL:
            e->maxHp = 250.0f * hpMult;
            e->baseSpeed = 70.0f;
            e->armor = 10 * armorMult;
            e->energyResist = 70 * armorMult;
            e->goldValue = 35;
            e->aetherValue = (GetRandomValue(0, 100) < 15) ? 2 : 0;
            e->xpValue = 55;
            break;
        case ENEMY_HEALER:
            e->maxHp = 300.0f * hpMult;
            e->baseSpeed = 65.0f;
            e->armor = 30 * armorMult;
            e->energyResist = 30 * armorMult;
            e->goldValue = 40;
            e->aetherValue = (GetRandomValue(0, 100) < 20) ? 3 : 1;
            e->xpValue = 60;
            break;
        case ENEMY_SPAWNER:
            e->maxHp = 1500.0f * hpMult;
            e->baseSpeed = 60.0f;
            e->armor = 50 * armorMult;
            e->energyResist = 50 * armorMult;
            e->goldValue = 150;
            e->aetherValue = 10;
            e->xpValue = 250;
            break;
        case ENEMY_MINION:
            e->maxHp = 50.0f * (1.0f + (game.currentWave - 1) * 0.1f);
            e->baseSpeed = 90.0f;
            e->armor = 5;
            e->energyResist = 5;
            e->goldValue = 3;
            e->aetherValue = 0;
            e->xpValue = 5;
            break;
        case ENEMY_BOSS:
            e->maxHp = 5000.0f * hpMult;
            e->baseSpeed = 60.0f;
            e->armor = 60 * armorMult;
            e->energyResist = 60 * armorMult;
            e->goldValue = 500;
            e->aetherValue = 50;
            e->xpValue = 1000;
            break;
    }
    e->hp = e->maxHp;
    e->speed = e->baseSpeed;
}

float CalculateDamage(float baseDamage, DamageType type, Enemy* target) {
    float damage = baseDamage;
    float energyMult = 1.0f, physicalMult = 1.0f;
    for (int i = 0; i < target->statusCount; i++) {
        if (target->status[i].type == STATUS_WEAKEN)
            energyMult *= target->status[i].intensity;
        if (target->status[i].type == STATUS_MELTED_ARMOR || target->status[i].type == STATUS_BRITTLE)
            physicalMult *= target->status[i].intensity;
    }
    switch (type) {
        case DMG_PHYSICAL:
            damage *= physicalMult;
            damage *= (100.0f / (100.0f + fmaxf(target->armor, -99.0f)));
            break;
        case DMG_ENERGY:
            damage *= energyMult;
            damage *= (100.0f / (100.0f + fmaxf(target->energyResist, -99.0f)));
            break;
        case DMG_TRUE:
            break;
    }
    return damage > 0.5f ? damage : 0.5f;
}

void ApplyStatusEffect(Enemy* enemy, StatusEffect type, float duration, float intensity) {
    if (!enemy || !enemy->active || type == STATUS_NONE) return;

    if (enemy->type == ENEMY_BOSS) {
        duration *= 0.5f;
        if (type == STATUS_STUN) duration *= 0.3f;
        if (type == STATUS_SLOW) intensity *= 0.5f;
    }

    for (int i = 0; i < enemy->statusCount; i++) {
        if (enemy->status[i].type == type) {
            if (intensity > enemy->status[i].intensity) {
                enemy->status[i].intensity = intensity;
                enemy->status[i].duration = duration;
                enemy->status[i].timer = 0;
            } else if (intensity == enemy->status[i].intensity) {
                float remaining = enemy->status[i].duration - enemy->status[i].timer;
                if (duration > remaining) {
                    enemy->status[i].duration = duration;
                    enemy->status[i].timer = 0;
                }
            }
            return;
        }
    }
    if (enemy->statusCount < MAX_STATUS_EFFECTS) {
        enemy->status[enemy->statusCount] = (ActiveStatus){type, duration, intensity, 0};
        enemy->statusCount++;
    }
}

void ProcessStatusEffects(Enemy* enemy, float dt) {
    float damage_over_time = 0.0f;
    float speed_multiplier = 1.0f;
    bool stunned = false;

    // Reset visual state
    Color baseColor;
    switch (enemy->type) {
        case ENEMY_BASIC: baseColor = MAROON; break;
        case ENEMY_FAST: baseColor = ORANGE; break;
        case ENEMY_TANK: baseColor = DARKBROWN; break;
        case ENEMY_ETHEREAL: baseColor = Fade(COLOR_ENERGY, 0.8f); break;
        case ENEMY_HEALER: baseColor = LIME; break;
        case ENEMY_SPAWNER: baseColor = PURPLE; break;
        case ENEMY_MINION: baseColor = DARKGRAY; break;
        case ENEMY_BOSS: baseColor = RED; break;
        default: baseColor = GRAY;
    }
    enemy->visualTint = baseColor;
    enemy->visualHasOutline = false;
    enemy->visualOutlineColor = BLACK;

    for (int i = 0; i < enemy->statusCount; ) {
        ActiveStatus* effect = &enemy->status[i];
        effect->timer += dt;

        if (effect->timer >= effect->duration) {
            if (i < enemy->statusCount - 1)
                enemy->status[i] = enemy->status[enemy->statusCount - 1];
            enemy->statusCount--;
            continue;
        }

        switch (effect->type) {
            case STATUS_BURN:
                damage_over_time += effect->intensity * dt;
                // Burn visual: flicker orange tint
                if ((int)(game.globalTime * 15) % 2 == 0)
                    enemy->visualTint = ColorTint(enemy->visualTint, ORANGE);
                break;
            case STATUS_STUN:
                stunned = true;
                enemy->visualTint = ColorTint(enemy->visualTint, SKYBLUE);
                break;
            case STATUS_SLOW:
            case STATUS_BRITTLE:
                enemy->visualTint = ColorAlphaBlend(enemy->visualTint, COLOR_CRYO, Fade(WHITE, 0.5f));
                break;
            case STATUS_WEAKEN:
                enemy->visualHasOutline = true;
                enemy->visualOutlineColor = COLOR_ENERGY;
                break;
            case STATUS_MELTED_ARMOR:
                enemy->visualHasOutline = true;
                enemy->visualOutlineColor = COLOR_PHYSICAL;
                break;
            default: break;
        }
        i++;
    }

    if (damage_over_time > 0)
        enemy->hp -= damage_over_time;

    if (stunned)
        enemy->speed = 0;
    else
        enemy->speed = enemy->baseSpeed * speed_multiplier;
}

void HandleEnemyDeath(int enemyIndex, int sourceTowerIndex) {
    if (enemyIndex < 0 || enemyIndex >= MAX_ENEMIES) return;
    Enemy* e = &game.enemies[enemyIndex];
    if (!e->active) return;

    e->active = false;

    game.gold += e->goldValue;
    game.aether += e->aetherValue;

    AddFloatingTextFmt(e->position, COLOR_GOLD, false, "+%dG", e->goldValue);
    if (e->aetherValue > 0)
        AddFloatingTextFmt((Vector2){e->position.x, e->position.y + 20}, COLOR_AETHER_RES, false, "+%dA", e->aetherValue);
    SpawnParticles(e->position, 40, RED, Fade(GRAY, 0.0f), 100.0f, 8.0f, 2.0f, true);

    if (e->type == ENEMY_SPAWNER) {
        for (int i = 0; i < 4; i++) {
            Vector2 offset = {GetRandomValue(-20, 20), GetRandomValue(-20, 20)};
            SpawnEnemy(ENEMY_MINION, Vector2Add(e->position, offset));
        }
    }

    if (sourceTowerIndex >= 0 && sourceTowerIndex < MAX_TOWERS) {
        GrantXP(sourceTowerIndex, e->xpValue);
        game.towers[sourceTowerIndex].kills++;
    }

    if (sourceTowerIndex == -2)
        GrantHeroXP(e->xpValue);
    else
        GrantHeroXP((int)(e->xpValue * 0.4f));
}
