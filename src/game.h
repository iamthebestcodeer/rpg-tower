#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <stdarg.h>
#include <float.h>

//----------------------------------------------------------------------------------
// Configuration Constants
//----------------------------------------------------------------------------------

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 800
#define GAME_AREA_WIDTH 960
#define UI_WIDTH (SCREEN_WIDTH - GAME_AREA_WIDTH)

#define TILE_SIZE 40
#define MAP_WIDTH (GAME_AREA_WIDTH / TILE_SIZE)   // 24
#define MAP_HEIGHT (SCREEN_HEIGHT / TILE_SIZE)    // 20

// Entity Limits
#define MAX_TOWERS 100
#define MAX_ENEMIES 300
#define MAX_PROJECTILES 500
#define MAX_PARTICLES 2200
#define MAX_FLOATING_TEXT 100
#define MAX_WAYPOINTS 50
#define MAX_CHAIN_HITS 10

// Game Balance
#define STARTING_LIVES 20
#define STARTING_GOLD 350
#define STARTING_AETHER 0
#define WAVE_INTERVAL 25.0f
#define TOWER_BASE_MAX_LEVEL 3

// Spatial Grid
#define GRID_COLS MAP_WIDTH
#define GRID_ROWS MAP_HEIGHT
#define MAX_ENEMIES_PER_CELL 32

// Aesthetics
#define COLOR_BG            (Color){ 10, 10, 20, 255 }
#define COLOR_UI_BG         (Color){ 20, 20, 35, 255 }
#define COLOR_UI_ACCENT     (Color){ 35, 35, 60, 255 }
#define COLOR_GRID          (Color){ 40, 40, 70, 100 }
#define COLOR_PATH          (Color){ 45, 30, 60, 255 }
#define COLOR_GRASS_1       (Color){ 15, 30, 25, 255 }
#define COLOR_GRASS_2       (Color){ 20, 35, 30, 255 }

#define COLOR_ENERGY        (Color){ 0, 220, 255, 255 }
#define COLOR_PHYSICAL      (Color){ 255, 140, 0, 255 }
#define COLOR_CRYO          (Color){ 135, 206, 250, 255 }
#define COLOR_TESLA         (Color){ 255, 0, 255, 255 }

#define COLOR_GOLD          (Color){ 255, 215, 0, 255 }
#define COLOR_AETHER_RES    (Color){ 180, 100, 255, 255 }
#define COLOR_XP            (Color){ 50, 255, 50, 255 }

#define COLOR_DANGER        (Color){ 255, 50, 50, 255 }
#define COLOR_TEXT_PRIMARY  (Color){ 245, 245, 255, 255 }
#define COLOR_TEXT_MUTED    (Color){ 150, 150, 180, 255 }

//----------------------------------------------------------------------------------
// Enums and Structures
//----------------------------------------------------------------------------------

typedef enum {
    GS_TITLE,
    GS_PLAYING,
    GS_PAUSED,
    GS_GAME_OVER,
    GS_LEVEL_UP_HERO
} GameState;

typedef enum {
    TOWER_NONE = -1,
    TOWER_PULSE,
    TOWER_CANNON,
    TOWER_CRYO,
    TOWER_TESLA,
    TOWER_T4_PULSE_REPEATER,
    TOWER_T4_PULSE_SNIPER,
    TOWER_T4_CANNON_MORTAR,
    TOWER_T4_CANNON_VULCAN,
    TOWER_T4_CRYO_BLIZZARD,
    TOWER_T4_CRYO_FREEZER,
    TOWER_T4_TESLA_CHAIN,
    TOWER_T4_TESLA_STORM,
} TowerType;

typedef enum {
    ENEMY_BASIC,
    ENEMY_FAST,
    ENEMY_TANK,
    ENEMY_ETHEREAL,
    ENEMY_HEALER,
    ENEMY_SPAWNER,
    ENEMY_MINION,
    ENEMY_BOSS
} EnemyType;

typedef enum {
    DMG_PHYSICAL,
    DMG_ENERGY,
    DMG_TRUE
} DamageType;

typedef enum {
    STATUS_NONE = 0,
    STATUS_SLOW = 1 << 0,
    STATUS_BURN = 1 << 1,
    STATUS_STUN = 1 << 2,
    STATUS_WEAKEN = 1 << 3,
    STATUS_BRITTLE = 1 << 4,
    STATUS_MELTED_ARMOR = 1 << 5
} StatusEffect;

typedef struct {
    StatusEffect type;
    float duration;
    float intensity;
    float timer;
} ActiveStatus;

#define MAX_STATUS_EFFECTS 6

typedef struct {
    bool visible;
    const char* title;
    const char* description;
    Rectangle bounds;
} Tooltip;

typedef struct {
    Vector2 position;
    char text[32];
    Color color;
    float lifetime;
    float velocityY;
    bool active;
    bool critical;
} FloatingText;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    Color startColor;
    Color endColor;
    float life;
    float maxLife;
    float startSize;
    float endSize;
    bool active;
    bool gravity;
} Particle;

typedef struct {
    Vector2 position;
    float speed;
    float damage;
    DamageType damageType;
    float aoeRadius;
    TowerType sourceType;
    int sourceTowerIndex;

    int targetIndex;
    int targetEnemyId;

    Vector2 targetPosition;

    bool active;
    int chainCount;
    int hitHistory[MAX_CHAIN_HITS];
    int hitCount;

    StatusEffect applyStatus;
    float statusDuration;
    float statusIntensity;

    // Lifetime for mortars (to avoid infinite flight)
    float lifetime;
} Projectile;

typedef struct Enemy {
    Vector2 position;
    EnemyType type;
    int id;
    float hp;
    float maxHp;
    float speed;
    float baseSpeed;
    float armor;
    float energyResist;
    int goldValue;
    int aetherValue;
    int xpValue;
    int waypointIndex;
    bool active;
    float abilityTimer;

    ActiveStatus status[MAX_STATUS_EFFECTS];
    int statusCount;

    // Precomputed visual state (for batch drawing)
    Color visualTint;
    bool visualHasOutline;
    Color visualOutlineColor;
} Enemy;

typedef struct {
    int level;
    int xp;
    int xpToNextLevel;
    float damage;
    float range;
    float fireRate;
} TowerStats;

typedef enum {
    TARGET_FIRST,
    TARGET_CLOSEST,
    TARGET_STRONGEST,
    TARGET_WEAKEST
} TargetingMode;
#define NUM_TARGETING_MODES 4

typedef struct {
    Vector2 position;
    TowerType type;
    TowerStats stats;
    DamageType damageType;
    float cooldownTimer;

    int targetIndex;
    int targetEnemyId;

    float rotation;
    float desiredRotation;
    float visualRecoil;
    bool active;
    TargetingMode targetingMode;
    int kills;
    int totalCost;

    // For throttled target search
    float targetSearchTimer;
} Tower;

typedef enum {
    SKILL_VIGOR,
    SKILL_AGILITY,
    SKILL_BURST_MASTERY,
    SKILL_LEADERSHIP
} HeroSkill;
#define NUM_HERO_SKILLS 4

typedef struct {
    Vector2 position;
    int level;
    int xp;
    int xpToNextLevel;
    int skillPoints;
    float speed;
    int attackDamage;
    float attackRange;
    float attackCooldown;
    float currentCooldown;
    float dashCooldown;
    float currentDashCooldown;
    float dashTimer;
    Vector2 dashDirection;
    Vector2 lastMovementDirection;
    float burstCooldown;
    float currentBurstCooldown;
    float burstDamage;
    float burstRange;
    int skills[NUM_HERO_SKILLS];
} Hero;

typedef struct {
    int tiles[MAP_HEIGHT][MAP_WIDTH];
    Vector2 waypoints[MAX_WAYPOINTS];
    int waypointCount;
} GameMap;

typedef struct {
    int indices[MAX_ENEMIES_PER_CELL];
    int count;
} GridCell;

typedef struct {
    GameState state;
    GameMap map;
    Hero hero;
    Camera2D camera;

    Tower towers[MAX_TOWERS];
    Enemy enemies[MAX_ENEMIES];
    Projectile projectiles[MAX_PROJECTILES];
    Particle particles[MAX_PARTICLES];
    FloatingText floatingTexts[MAX_FLOATING_TEXT];

    // Free-list heads
    int nextFreeProjectile;
    int nextFreeParticle;
    int nextFreeFloatingText;

    int lives;
    int gold;
    int aether;
    int currentWave;
    float waveTimer;
    int enemiesToSpawn;
    float spawnTimer;
    bool waveActive;

    TowerType placingTower;
    int selectedTowerIndex;
    int enemyIdCounter;
    float globalTime;
    Tooltip tooltip;

    float screenShakeIntensity;
    float screenShakeTime;
    float screenShakeDuration;
    float dayNightCycle;
    Color environmentColor;

    RenderTexture2D mapRT;
    bool mapRTBuilt;

    bool occupied[MAP_HEIGHT][MAP_WIDTH];

    // Spatial grid
    GridCell enemyGrid[GRID_ROWS][GRID_COLS];
} GameData;

extern GameData game;

//----------------------------------------------------------------------------------
// Inline Helper
//----------------------------------------------------------------------------------

static inline Enemy *GetEnemyFromPair(int index, int id)
{
    if (index >= 0 && index < MAX_ENEMIES) {
        Enemy *e = &game.enemies[index];
        if (e->active && e->id == id) return e;
    }
    return NULL;
}

//----------------------------------------------------------------------------------
// Function Prototypes
//----------------------------------------------------------------------------------

void InitGame(void);
void InitMap(void);
void ResetGame(void);
void BuildStaticMapRT(void);

void UpdateGame(float dt);
void DrawGame(void);

void UpdatePlaying(float dt);
void UpdateHeroLevelUp(float dt);

void UpdateHero(float dt);
void HeroAttack(void);
void ApplyHeroSkills(void);
void UpdateTowers(float dt);
void UpdateEnemies(float dt);
void UpdateProjectiles(float dt);
void ManageWaves(float dt);
void HandleInput(void);
void UpdateEnvironment(float dt);

// Spatial grid
void RebuildEnemyGrid(void);
void GetEnemiesInRadius(Vector2 center, float radius, int* outIndices, int* outCount, int maxOut);

void SpawnEnemy(EnemyType type, Vector2 position);
bool PlaceTower(int x, int y, TowerType type);
void SellTower(int towerIndex);
bool UpgradeTower(Tower* t, TowerType newType);
void ConfigureTowerStats(Tower* tower);
void FireProjectile(Tower* tower, Enemy* target);
void HandleProjectileImpact(Projectile* p, Enemy* primaryTarget);
void ApplyDamageAndEffects(Projectile* p, Enemy* target, bool isCritical);

// Draw-phase profiling (--bench only): when g_drawTrace is set, DrawGame
// accumulates per-phase frame time in g_drawTraceMs so the benchmark can
// attribute draw cost precisely.
#define DRAW_TRACE_PHASES 10
extern bool g_drawTrace;
extern double g_drawTraceMs[DRAW_TRACE_PHASES];
double NowMs(void); // high-resolution monotonic clock (ms), used by the bench + draw trace

void DrawMap(void);
void DrawEntities(void);
void DrawTowers(void);
void DrawEnemies(void);
void DrawHero(void);
void DrawProjectiles(void);

void DrawUI(bool interactive);

// Batched rendering: emit primitives into the caller's rlBegin() block so
// thousands of objects share one rlgl batch instead of one draw call each.
// EmitCircleFan matches raylib's DrawCircleV geometry with adaptive segment
// count and zero per-frame trig. EmitRect emits a filled rect. DrawTextBatched
// draws text with DrawTextEx placement via DrawTexturePro per glyph (always
// renders on this raylib 6.0 build, unlike raw immediate-mode quads).
void EmitCircleFan(Vector2 center, float radius, Color color);
void EmitRect(Rectangle rec, Color color);
void DrawTextBatched(Font font, const char* text, Vector2 position, float fontSize, float spacing, Color tint);
void DrawHeroStatus(void);
void DrawBuildMenu(bool interactive);
void DrawTowerInspector(bool interactive);
void DrawTowerUpgradePaths(Tower* t, bool interactive);
void DrawTooltip(void);
void SetTooltip(const char* title, const char* description, Rectangle bounds);

void GrantXP(int towerIndex, int xp);
void GrantHeroXP(int xp);
void LevelUpTower(int towerIndex);
void LevelUpHero(void);
void HandleEnemyDeath(int enemyIndex, int sourceTowerIndex);
float CalculateDamage(float baseDamage, DamageType type, Enemy* target);
void ApplyStatusEffect(Enemy* enemy, StatusEffect type, float duration, float intensity);
void ProcessStatusEffects(Enemy* enemy, float dt);

void UpdateVFX(float dt);
void DrawVFX(void);
void InitVFX(void);
void UnloadVFX(void);
void SpawnParticles(Vector2 position, int count, Color startColor, Color endColor, float speed, float sizeStart, float sizeEnd, bool gravity);
void AddFloatingText(Vector2 position, const char* text, Color color, bool critical);
void AddFloatingTextFmt(Vector2 position, Color color, bool critical, const char* fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 4, 5)))
#endif
;
void ScreenShake(float intensity, float duration);

Vector2 WorldToTile(Vector2 worldPos);
Vector2 TileToWorldCenter(int x, int y);
Vector2 ClosestPointOnSegment(Vector2 p, Vector2 a, Vector2 b);
bool IsTileBuildable(int x, int y);
int GetTowerCost(TowerType type);
int GetTowerAetherCost(TowerType type);
const char* GetTowerName(TowerType type);
const char* GetTowerDescription(TowerType type);
Color GetTowerColor(TowerType type);
Enemy* GetEnemyById(int id);
int GetEnemyIndexById(int id);
float LerpAngle(float start, float end, float amount);
float GetAimToleranceDegrees(TowerType type);

bool GuiButton(Rectangle bounds, const char* text, bool selected, bool enabled);
float GetAngleDifference(float a, float b);

#endif // GAME_H
