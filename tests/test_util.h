// Minimal assertion framework + shared helpers for the game test suite.
#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include "game.h"
#include "raylib_stub.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

extern int g_checks;
extern int g_failures;

#define CHECK(cond) do { \
    g_checks++; \
    if (!(cond)) { g_failures++; \
        printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define CHECK_NEAR(a, b, eps) do { \
    double _a = (double)(a), _b = (double)(b); \
    g_checks++; \
    if (fabs(_a - _b) > (double)(eps)) { g_failures++; \
        printf("  FAIL %s:%d: %s ~= %s (%f vs %f)\n", __FILE__, __LINE__, #a, #b, _a, _b); } \
} while (0)

#define CHECK_STREQ(a, b) do { \
    const char *_a = (a), *_b = (b); \
    g_checks++; \
    if (!_a || !_b || strcmp(_a, _b) != 0) { g_failures++; \
        printf("  FAIL %s:%d: %s == %s (\"%s\" vs \"%s\")\n", __FILE__, __LINE__, \
               #a, #b, _a ? _a : "(null)", _b ? _b : "(null)"); } \
} while (0)

// Reset the world to a clean gameplay state (fresh input, fresh game data,
// fresh camera/timers). Note ResetGame() leaves the camera alone, and other
// tests may have moved it (screen shake), so it is restored here.
static inline void ResetForTest(void) {
    StubResetInput();
    StubResetDrawLog();
    StubResetRlglLog();
    ResetGame();
    game.camera.target = (Vector2){0, 0};
    game.camera.offset = (Vector2){0, 0};
    game.camera.rotation = 0.0f;
    game.camera.zoom = 1.0f;
    game.globalTime = 0.0f;
    RebuildEnemyGrid();
}

// Manually activate an enemy at an exact world position with deterministic
// stats. SpawnEnemy() snaps off-path spawns onto the path waypoints, which
// makes exact-position assertions brittle; this helper bypasses snapping.
// Call RebuildEnemyGrid() afterwards to insert it into the spatial grid.
static inline int PlaceEnemyExact(int slot, EnemyType type, float x, float y, float hp) {
    Enemy* e = &game.enemies[slot];
    memset(e, 0, sizeof(Enemy));
    e->active = true;
    e->type = type;
    e->id = game.enemyIdCounter++;
    e->position = (Vector2){x, y};
    e->maxHp = hp;
    e->hp = hp;
    e->baseSpeed = 80.0f;
    e->speed = 80.0f;
    e->armor = 10.0f;
    e->energyResist = 10.0f;
    e->goldValue = 10;
    e->xpValue = 20;
    e->waypointIndex = 0;
    return slot;
}

// Spawn an enemy and return its index (the one with the highest id).
static inline int SpawnEnemyAt(EnemyType type, float x, float y) {
    Vector2 pos = { x, y };
    SpawnEnemy(type, pos);
    int best = -1, bestId = -1;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (game.enemies[i].active && game.enemies[i].id > bestId) {
            bestId = game.enemies[i].id;
            best = i;
        }
    }
    return best;
}

static inline int CountActiveEnemies(void) {
    int n = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) if (game.enemies[i].active) n++;
    return n;
}

static inline int CountActiveTowers(void) {
    int n = 0;
    for (int i = 0; i < MAX_TOWERS; i++) if (game.towers[i].active) n++;
    return n;
}

static inline int CountActiveProjectiles(void) {
    int n = 0;
    for (int i = 0; i < MAX_PROJECTILES; i++) if (game.projectiles[i].active) n++;
    return n;
}

static inline int CountActiveParticles(void) {
    int n = 0;
    for (int i = 0; i < MAX_PARTICLES; i++) if (game.particles[i].active) n++;
    return n;
}

static inline float DistSqr(Vector2 a, Vector2 b) {
    return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y);
}

// ---- draw-call log helpers ----
// These search the stub's recorded draw calls. Call StubResetDrawLog() right
// before the draw you want to assert on, or rely on ResetForTest() having
// cleared it.

static inline int StubFindText(const char* text) {
    for (int i = 0; i < StubDrawLogCount(); i++) {
        StubDrawCall c = StubDrawLogAt(i);
        if (c.kind == STUB_DRAW_TEXT && c.text[0] && strcmp(c.text, text) == 0) return i;
    }
    return -1;
}

static inline int StubFindCircle(StubDrawKind kind, float x, float y, float r, float tol) {
    for (int i = 0; i < StubDrawLogCount(); i++) {
        StubDrawCall c = StubDrawLogAt(i);
        if (c.kind == kind && fabsf(c.x - x) <= tol && fabsf(c.y - y) <= tol &&
            fabsf(c.w - r) <= tol) return i;
    }
    return -1;
}

#endif // TEST_UTIL_H
