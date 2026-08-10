#include "game.h"

//----------------------------------------------------------------------------------
// Projectiles
//----------------------------------------------------------------------------------

void FireProjectile(Tower* tower, Enemy* target) {
    // Free-list allocation
    int index = -1;
    for (int i = game.nextFreeProjectile; i < MAX_PROJECTILES; i++) {
        if (!game.projectiles[i].active) { index = i; break; }
    }
    if (index == -1) {
        for (int i = 0; i < game.nextFreeProjectile; i++) {
            if (!game.projectiles[i].active) { index = i; break; }
        }
    }
    if (index == -1) return;

    game.nextFreeProjectile = (index + 1) % MAX_PROJECTILES;

    Projectile* p = &game.projectiles[index];
    memset(p, 0, sizeof(Projectile));
    p->active = true;
    p->position = tower->position;
    p->damage = tower->stats.damage;
    p->damageType = tower->damageType;
    p->sourceType = tower->type;
    p->sourceTowerIndex = (int)(tower - game.towers);
    p->targetIndex = (int)(target - game.enemies);
    p->targetEnemyId = target->id;
    p->targetPosition = target->position;
    p->lifetime = 10.0f; // default, will be changed for mortars

    switch (tower->type) {
        case TOWER_PULSE: case TOWER_T4_PULSE_REPEATER:
            p->speed = 650.0f;
            break;
        case TOWER_T4_PULSE_SNIPER:
            p->speed = 1200.0f;
            break;
        case TOWER_CANNON: case TOWER_T4_CANNON_VULCAN: case TOWER_T4_CANNON_MORTAR:
            p->speed = (tower->type == TOWER_CANNON) ? 350.0f : (tower->type == TOWER_T4_CANNON_VULCAN ? 700.0f : 250.0f);
            if (tower->type == TOWER_CANNON) p->aoeRadius = 50.0f + tower->stats.level * 10.0f;
            if (tower->type == TOWER_T4_CANNON_MORTAR) {
                p->aoeRadius = 150.0f;
                p->lifetime = 6.0f; // max flight time
                // predictive targeting
                if (target->baseSpeed > 0 && target->waypointIndex < game.map.waypointCount) {
                    Vector2 nextWaypoint = game.map.waypoints[target->waypointIndex];
                    if (Vector2DistanceSqr(target->position, nextWaypoint) > 1.0f) {
                        Vector2 direction = Vector2Normalize(Vector2Subtract(nextWaypoint, target->position));
                        Vector2 targetVelocity = Vector2Scale(direction, target->baseSpeed);
                        float distance = Vector2Distance(p->position, target->position);
                        if (p->speed > 0) {
                            float estimatedTime = distance / p->speed;
                            p->targetPosition = Vector2Add(target->position, Vector2Scale(targetVelocity, estimatedTime));
                        }
                    }
                }
            }
            p->applyStatus = STATUS_BURN;
            p->statusDuration = 5.0f;
            p->statusIntensity = 10.0f + tower->stats.level * 5.0f;
            break;
        case TOWER_TESLA: case TOWER_T4_TESLA_STORM:
            p->speed = 900.0f;
            p->applyStatus = STATUS_STUN;
            p->statusDuration = 0.6f;
            p->statusIntensity = 20.0f + tower->stats.level * 7.0f;
            break;
        case TOWER_T4_TESLA_CHAIN:
            p->speed = 900.0f;
            p->chainCount = 3 + tower->stats.level;
            break;
        default:
            p->active = false;
            return;
    }
}

void UpdateProjectiles(float dt) {
    // Enemies moved during UpdateEnemies, so the grid rebuilt for tower
    // targeting at the start of the frame is now stale. Rebuild it here so
    // projectile-impact AoE queries (HandleProjectileImpact) use current cell
    // membership instead of omitting enemies that crossed a cell boundary.
    RebuildEnemyGrid();

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!game.projectiles[i].active) continue;
        Projectile* p = &game.projectiles[i];

        // Lifetime for mortars
        p->lifetime -= dt;
        if (p->lifetime <= 0) {
            p->active = false;
            continue;
        }

        Enemy* targetEnemy = NULL;
        Vector2 currentTargetPos = {0};
        bool targetIsReachable = false;

        if (p->sourceType == TOWER_T4_CANNON_MORTAR) {
            currentTargetPos = p->targetPosition;
            targetIsReachable = true;
            targetEnemy = GetEnemyFromPair(p->targetIndex, p->targetEnemyId);
        } else {
            targetEnemy = GetEnemyFromPair(p->targetIndex, p->targetEnemyId);
            if (targetEnemy) {
                currentTargetPos = targetEnemy->position;
                targetIsReachable = true;
            }
        }

        if (targetIsReachable) {
            float collisionRadius = 10.0f;
            if (p->sourceType == TOWER_CANNON || p->sourceType == TOWER_T4_CANNON_MORTAR) collisionRadius = 15.0f;
            if (p->sourceType == TOWER_T4_PULSE_SNIPER) collisionRadius = 5.0f;

            Vector2 toTarget = Vector2Subtract(currentTargetPos, p->position);
            float distSqr = Vector2LengthSqr(toTarget);
            float travelThisFrame = p->speed * dt;
            float reachSqr = (travelThisFrame + collisionRadius) * (travelThisFrame + collisionRadius);

            if (distSqr <= reachSqr) {
                p->position = currentTargetPos;
                p->active = false;
                Enemy* primaryImpactTarget = targetEnemy;
                if (p->sourceType == TOWER_T4_CANNON_MORTAR) {
                    if (primaryImpactTarget == NULL || !primaryImpactTarget->active || Vector2Distance(p->position, primaryImpactTarget->position) > p->aoeRadius)
                        primaryImpactTarget = NULL;
                } else {
                    if (primaryImpactTarget == NULL || !primaryImpactTarget->active)
                        primaryImpactTarget = NULL;
                }
                HandleProjectileImpact(p, primaryImpactTarget);
                continue;
            }

            // Move: only one sqrt
            float dist = sqrtf(distSqr);
            Vector2 direction = (dist > 0.001f) ? Vector2Scale(toTarget, 1.0f/dist) : (Vector2){0};
            Vector2 velocity = Vector2Scale(direction, travelThisFrame);
            p->position = Vector2Add(p->position, velocity);
        } else {
            p->active = false;
        }
    }
}

void HandleProjectileImpact(Projectile* p, Enemy* primaryTarget) {
    Color particleColor = GetTowerColor(p->sourceType);
    int particleCount = 10;
    float particleSpeed = 60.0f;
    float particleSize = 5.0f;
    bool gravity = (p->damageType == DMG_PHYSICAL);

    if (p->aoeRadius > 0) {
        particleCount = 40;
        particleSpeed = 150.0f;
        particleSize = 9.0f;
        ScreenShake(p->aoeRadius * 0.05f, 0.25f);
    }
    SpawnParticles(p->position, particleCount, particleColor, Fade(YELLOW, 0.0f), particleSpeed, particleSize, 2.0f, gravity);

    if (primaryTarget && p->hitCount < MAX_CHAIN_HITS) {
        bool alreadyCounted = false;
        for (int k = 0; k < p->hitCount; k++) {
            if (primaryTarget->id == p->hitHistory[k]) { alreadyCounted = true; break; }
        }
        if (!alreadyCounted)
            p->hitHistory[p->hitCount++] = primaryTarget->id;
    }

    // Chain lightning - use spatial grid and avoid reusing the just-freed slot
    if (p->sourceType == TOWER_T4_TESLA_CHAIN && p->chainCount > 0) {
        int sourceIdx = (int)(p - game.projectiles);
        int nextTargetId = -1;
        float minDistSqr = 150.0f * 150.0f;
        int nearby[MAX_ENEMIES];
        int nearbyCount = 0;
        GetEnemiesInRadius(p->position, 150.0f, nearby, &nearbyCount, MAX_ENEMIES);
        for (int n = 0; n < nearbyCount; n++) {
            int j = nearby[n];
            bool alreadyHit = false;
            for (int k = 0; k < p->hitCount; k++) {
                if (game.enemies[j].id == p->hitHistory[k]) { alreadyHit = true; break; }
            }
            if (alreadyHit) continue;
            float distSqr = Vector2DistanceSqr(p->position, game.enemies[j].position);
            if (distSqr < minDistSqr) {
                minDistSqr = distSqr;
                nextTargetId = game.enemies[j].id;
            }
        }
        if (nextTargetId != -1) {
            int idx = -1;
            for (int k = 0; k < MAX_PROJECTILES; k++) {
                if (k == sourceIdx) continue;
                if (!game.projectiles[k].active) { idx = k; break; }
            }
            if (idx != -1) {
                Projectile tmp = *p;
                Projectile* chainP = &game.projectiles[idx];
                *chainP = tmp;
                chainP->active = true;
                chainP->position = p->position;
                int nextIndex = -1;
                for (int j = 0; j < MAX_ENEMIES; j++) {
                    if (game.enemies[j].active && game.enemies[j].id == nextTargetId) { nextIndex = j; break; }
                }
                chainP->targetIndex = nextIndex;
                chainP->targetEnemyId = nextTargetId;
                chainP->damage *= 0.7f;
                chainP->chainCount--;
            }
        }
    }

    // Damage
    if (p->aoeRadius > 0) {
        float aoeR2 = p->aoeRadius * p->aoeRadius;
        // Use grid to find nearby enemies for AoE
        int nearby[MAX_ENEMIES];
        int nearbyCount = 0;
        GetEnemiesInRadius(p->position, p->aoeRadius, nearby, &nearbyCount, MAX_ENEMIES);
        for (int k = 0; k < nearbyCount; k++) {
            int j = nearby[k];
            if (Vector2DistanceSqr(p->position, game.enemies[j].position) <= aoeR2) {
                bool critical = GetRandomValue(1, 100) <= 5;
                ApplyDamageAndEffects(p, &game.enemies[j], critical);
            }
        }
    } else {
        if (primaryTarget && primaryTarget->active) {
            bool critical = GetRandomValue(1, 100) <= 5;
            ApplyDamageAndEffects(p, primaryTarget, critical);
        }
    }
}

void ApplyDamageAndEffects(Projectile* p, Enemy* target, bool isCritical) {
    if (!target->active) return;
    float actualDamage = CalculateDamage(p->damage, p->damageType, target);
    if (isCritical) actualDamage *= 2.0f;
    target->hp -= actualDamage;

    Color textColor = (p->damageType == DMG_ENERGY) ? COLOR_ENERGY : COLOR_PHYSICAL;
    if (isCritical) textColor = YELLOW;
    AddFloatingTextFmt(target->position, textColor, isCritical, "%.0f", actualDamage);

    if (p->applyStatus != STATUS_NONE) {
        bool apply = true;
        float intensity = p->statusIntensity;
        if (p->applyStatus == STATUS_STUN) {
            if (GetRandomValue(0, 100) >= (int)p->statusIntensity)
                apply = false;
            intensity = 1.0f;
        }
        if (p->applyStatus == STATUS_BURN) {
            ApplyStatusEffect(target, STATUS_MELTED_ARMOR, p->statusDuration, 1.15f);
        }
        if (apply)
            ApplyStatusEffect(target, p->applyStatus, p->statusDuration, intensity);
    }

    if (target->hp <= 0) {
        int targetIndex = (int)(target - game.enemies);
        HandleEnemyDeath(targetIndex, p->sourceTowerIndex);
    }
}
