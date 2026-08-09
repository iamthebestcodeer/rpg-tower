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
#define MAX_PARTICLES 3000
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
#define GRID_COLS 24
#define GRID_ROWS 20
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
    bool showGrid;
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

static GameData game = {0};

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

void DrawMap(void);
void DrawEntities(void);
void DrawTowers(void);
void DrawEnemies(void);
void DrawHero(void);
void DrawProjectiles(void);

void DrawUI(void);
void DrawHeroStatus(void);
void DrawBuildMenu(void);
void DrawTowerInspector(void);
void DrawTowerUpgradePaths(Tower* t);
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
void SpawnParticles(Vector2 position, int count, Color startColor, Color endColor, float speed, float sizeStart, float sizeEnd, bool gravity);
void AddFloatingText(Vector2 position, const char* text, Color color, bool critical);
void AddFloatingTextFmt(Vector2 position, Color color, bool critical, const char* fmt, ...);
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
static inline Enemy* GetEnemyFromPair(int index, int id);

//----------------------------------------------------------------------------------
// Main
//----------------------------------------------------------------------------------

int main(void) {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Aetherium Vanguard - OPTIMIZED");
    SetRandomSeed((unsigned int)time(NULL));

    InitGame();

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.1f) dt = 0.1f;
        game.globalTime += dt;
        UpdateGame(dt);
        DrawGame();
    }

    if (game.mapRTBuilt) UnloadRenderTexture(game.mapRT);
    CloseWindow();
    return 0;
}

//----------------------------------------------------------------------------------
// Initialization
//----------------------------------------------------------------------------------

void InitGame(void) {
    InitMap();
    ResetGame();
    game.state = GS_TITLE;

    game.camera.target = (Vector2){ 0, 0 };
    game.camera.offset = (Vector2){ 0, 0 };
    game.camera.rotation = 0.0f;
    game.camera.zoom = 1.0f;

    BuildStaticMapRT();
}

void ResetGame(void) {
    memset(game.towers, 0, sizeof(game.towers));
    memset(game.enemies, 0, sizeof(game.enemies));
    memset(game.projectiles, 0, sizeof(game.projectiles));
    memset(game.particles, 0, sizeof(game.particles));
    memset(game.floatingTexts, 0, sizeof(game.floatingTexts));
    memset(game.occupied, 0, sizeof(game.occupied));
    memset(game.enemyGrid, 0, sizeof(game.enemyGrid));

    game.lives = STARTING_LIVES;
    game.gold = STARTING_GOLD;
    game.aether = STARTING_AETHER;
    game.currentWave = 0;
    game.waveTimer = 10.0f;
    game.waveActive = false;
    game.placingTower = TOWER_NONE;
    game.selectedTowerIndex = -1;
    game.showGrid = false;
    game.enemyIdCounter = 1;
    game.dayNightCycle = 0.0f;
    game.screenShakeDuration = 0.0f;
    game.screenShakeTime = 0.0f;

    // Free-list initialization
    game.nextFreeProjectile = 0;
    game.nextFreeParticle = 0;
    game.nextFreeFloatingText = 0;

    Hero* h = &game.hero;
    memset(h, 0, sizeof(Hero));
    h->position = (Vector2){ 100, 100 };
    h->level = 1;
    h->xpToNextLevel = 250;
    h->attackRange = 65.0f;
    h->attackCooldown = 0.6f;
    h->dashCooldown = 6.0f;
    h->burstCooldown = 20.0f;
    h->lastMovementDirection = (Vector2){1, 0};
    ApplyHeroSkills();
}

void InitMap(void) {
    for (int y = 0; y < MAP_HEIGHT; y++)
        for (int x = 0; x < MAP_WIDTH; x++)
            game.map.tiles[y][x] = 0;

    int pathCoords[][2] = {
        {0, 3}, {1, 3}, {2, 3}, {3, 3}, {4, 3}, {5, 3}, {6, 3},
        {6, 4}, {6, 5}, {6, 6}, {6, 7}, {6, 8}, {6, 9},
        {5, 9}, {4, 9}, {3, 9}, {2, 9},
        {2, 10}, {2, 11}, {2, 12}, {2, 13}, {2, 14},
        {3, 14}, {4, 14}, {5, 14}, {6, 14}, {7, 14}, {8, 14}, {9, 14}, {10, 14}, {11, 14},
        {11, 13}, {11, 12}, {11, 11}, {11, 10}, {11, 9}, {11, 8}, {11, 7},
        {12, 7}, {13, 7}, {14, 7}, {15, 7}, {16, 7}, {17, 7},
        {17, 8}, {17, 9}, {17, 10}, {17, 11}, {17, 12},
        {18, 12}, {19, 12}, {20, 12}, {21, 12},
        {21, 11}, {21, 10}, {21, 9}, {21, 8}, {21, 7}, {21, 6}, {21, 5},
        {22, 5}, {23, 5}
    };
    int numPathTiles = sizeof(pathCoords) / sizeof(pathCoords[0]);
    for (int i = 0; i < numPathTiles; i++) {
        int x = pathCoords[i][0], y = pathCoords[i][1];
        if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT)
            game.map.tiles[y][x] = 1;
    }

    game.map.waypointCount = 0;
    game.map.waypoints[game.map.waypointCount++] = TileToWorldCenter(-2, 3);
    for (int i = 0; i < numPathTiles; i++) {
        bool directionChanged = false;
        if (i > 0 && i < numPathTiles - 1) {
            int dx_prev = pathCoords[i][0] - pathCoords[i-1][0];
            int dy_prev = pathCoords[i][1] - pathCoords[i-1][1];
            int dx_next = pathCoords[i+1][0] - pathCoords[i][0];
            int dy_next = pathCoords[i+1][1] - pathCoords[i][1];
            if (dx_prev != dx_next || dy_prev != dy_next)
                directionChanged = true;
        }
        if (directionChanged || i == numPathTiles - 1) {
            if (game.map.waypointCount < MAX_WAYPOINTS)
                game.map.waypoints[game.map.waypointCount++] = TileToWorldCenter(pathCoords[i][0], pathCoords[i][1]);
        }
    }
    if (game.map.waypointCount < MAX_WAYPOINTS)
        game.map.waypoints[game.map.waypointCount++] = TileToWorldCenter(MAP_WIDTH + 1, 5);
}

void BuildStaticMapRT(void) {
    if (game.mapRTBuilt) return;
    game.mapRT = LoadRenderTexture(GAME_AREA_WIDTH, SCREEN_HEIGHT);
    BeginTextureMode(game.mapRT);
    DrawRectangleGradientV(0, 0, GAME_AREA_WIDTH, SCREEN_HEIGHT,
                           COLOR_BG, ColorBrightness(COLOR_BG, 0.1f));
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            Rectangle tileRect = {x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE};
            Color tileColor = (game.map.tiles[y][x] == 1) ? COLOR_PATH :
                              ((x + y) % 2 == 0) ? COLOR_GRASS_1 : COLOR_GRASS_2;
            DrawRectangleRec(tileRect, tileColor);
            if (game.map.tiles[y][x] == 1) {
                Color edgeGlow = Fade(COLOR_AETHER_RES, 0.4f);
                if (y > 0 && game.map.tiles[y-1][x] == 0) DrawLineEx((Vector2){tileRect.x, tileRect.y}, (Vector2){tileRect.x+TILE_SIZE, tileRect.y}, 2.0f, edgeGlow);
                if (y < MAP_HEIGHT-1 && game.map.tiles[y+1][x] == 0) DrawLineEx((Vector2){tileRect.x, tileRect.y+TILE_SIZE}, (Vector2){tileRect.x+TILE_SIZE, tileRect.y+TILE_SIZE}, 2.0f, edgeGlow);
                if (x > 0 && game.map.tiles[y][x-1] == 0) DrawLineEx((Vector2){tileRect.x, tileRect.y}, (Vector2){tileRect.x, tileRect.y+TILE_SIZE}, 2.0f, edgeGlow);
                if (x < MAP_WIDTH-1 && game.map.tiles[y][x+1] == 0) DrawLineEx((Vector2){tileRect.x+TILE_SIZE, tileRect.y}, (Vector2){tileRect.x+TILE_SIZE, tileRect.y+TILE_SIZE}, 2.0f, edgeGlow);
            }
        }
    }
    EndTextureMode();
    game.mapRTBuilt = true;
}

//----------------------------------------------------------------------------------
// Update
//----------------------------------------------------------------------------------

void UpdateGame(float dt) {
    UpdateEnvironment(dt);
    UpdateVFX(dt);
    game.tooltip.visible = false;

    switch (game.state) {
        case GS_TITLE:
            if (IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                ResetGame();
                game.state = GS_PLAYING;
            }
            break;
        case GS_PLAYING:
            UpdatePlaying(dt);
            break;
        case GS_PAUSED:
            if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) game.state = GS_PLAYING;
            // Do NOT process input in pause to avoid placing towers etc.
            break;
        case GS_LEVEL_UP_HERO:
            UpdateHeroLevelUp(dt);
            break;
        case GS_GAME_OVER:
            if (IsKeyPressed(KEY_R)) {
                ResetGame();
                game.state = GS_PLAYING;
            }
            break;
    }
}

void UpdatePlaying(float dt) {
    if (IsKeyPressed(KEY_P)) {
        game.state = GS_PAUSED;
        return;
    }

    if (game.hero.skillPoints > 0) {
        game.state = GS_LEVEL_UP_HERO;
        return;
    }

    HandleInput();
    ManageWaves(dt);
    UpdateHero(dt);
    UpdateTowers(dt);
    UpdateEnemies(dt);
    UpdateProjectiles(dt);

    if (game.lives <= 0) {
        game.lives = 0;
        if (game.state != GS_GAME_OVER) {
            game.state = GS_GAME_OVER;
            ScreenShake(20.0f, 2.0f);
        }
    }
}

void UpdateEnvironment(float dt) {
    // Day/Night
    float cycleSpeed = 1.0f / 120.0f;
    game.dayNightCycle += dt * cycleSpeed;
    if (game.dayNightCycle >= 2.0f) game.dayNightCycle -= 2.0f;
    float lightLevel = (cosf(game.dayNightCycle * PI) + 1.0f) / 2.0f;
    Color dayColor = WHITE;
    Color nightColor = (Color){ 80, 80, 150, 255 };
    float clampedLightLevel = Clamp(lightLevel, 0.4f, 1.0f);
    game.environmentColor = ColorLerp(nightColor, dayColor, clampedLightLevel);

    // Screen shake
    if (game.screenShakeTime > 0) {
        game.screenShakeTime -= dt;
        float normalizedTime = Clamp(game.screenShakeTime / game.screenShakeDuration, 0.0f, 1.0f);
        float intensity = game.screenShakeIntensity * normalizedTime;
        game.camera.offset.x = GetRandomValue(-(int)intensity, (int)intensity);
        game.camera.offset.y = GetRandomValue(-(int)intensity, (int)intensity);
    } else {
        game.camera.offset.x = Lerp(game.camera.offset.x, 0, 10.0f * dt);
        game.camera.offset.y = Lerp(game.camera.offset.y, 0, 10.0f * dt);
    }
}

void UpdateHeroLevelUp(float dt) {
    (void)dt;
    if (game.hero.skillPoints == 0)
        game.state = GS_PLAYING;
}

void HandleInput(void) {
    if (IsKeyPressed(KEY_G)) game.showGrid = !game.showGrid;

    Vector2 mousePosScreen = GetMousePosition();
    Vector2 mousePosWorld = GetScreenToWorld2D(mousePosScreen, game.camera);

    if (mousePosScreen.x < GAME_AREA_WIDTH) {
        Vector2 tilePos = WorldToTile(mousePosWorld);
        int tileX = (int)tilePos.x, tileY = (int)tilePos.y;

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (game.placingTower != TOWER_NONE) {
                if (PlaceTower(tileX, tileY, game.placingTower)) {
                    if (!IsKeyDown(KEY_LEFT_SHIFT) && !IsKeyDown(KEY_RIGHT_SHIFT))
                        game.placingTower = TOWER_NONE;
                }
            } else {
                game.selectedTowerIndex = -1;
                for (int i = 0; i < MAX_TOWERS; i++) {
                    if (game.towers[i].active) {
                        Rectangle towerRect = {game.towers[i].position.x - TILE_SIZE/2, game.towers[i].position.y - TILE_SIZE/2, TILE_SIZE, TILE_SIZE};
                        if (CheckCollisionPointRec(mousePosWorld, towerRect)) {
                            game.selectedTowerIndex = i;
                            break;
                        }
                    }
                }
            }
        }
    }

    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) || (IsKeyPressed(KEY_ESCAPE) && game.state != GS_LEVEL_UP_HERO)) {
        game.placingTower = TOWER_NONE;
        game.selectedTowerIndex = -1;
    }

    if (IsKeyPressed(KEY_ONE)) { game.placingTower = TOWER_PULSE; game.selectedTowerIndex = -1; }
    if (IsKeyPressed(KEY_TWO)) { game.placingTower = TOWER_CANNON; game.selectedTowerIndex = -1; }
    if (IsKeyPressed(KEY_THREE)) { game.placingTower = TOWER_CRYO; game.selectedTowerIndex = -1; }
    if (IsKeyPressed(KEY_FOUR)) { game.placingTower = TOWER_TESLA; game.selectedTowerIndex = -1; }
}

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

void HeroAttack() {
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

//----------------------------------------------------------------------------------
// Towers
//----------------------------------------------------------------------------------

float GetAimToleranceDegrees(TowerType type) {
    switch (type) {
        case TOWER_T4_PULSE_REPEATER:
        case TOWER_T4_CANNON_VULCAN:
        case TOWER_PULSE:
            return 30.0f;
        case TOWER_T4_PULSE_SNIPER:
        case TOWER_CANNON:
        case TOWER_T4_CANNON_MORTAR:
            return 12.0f;
        case TOWER_CRYO:
        case TOWER_T4_CRYO_FREEZER:
            return 15.0f;
        default:
            return 20.0f;
    }
}

void UpdateTowers(float dt) {
    // Rebuild grid once per frame for all queries
    RebuildEnemyGrid();

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

            float angleDiff = fabsf(fmodf(t->rotation - t->desiredRotation + 180.0f, 360.0f) - 180.0f);
            float aimTolerance = GetAimToleranceDegrees(t->type);

            if (t->cooldownTimer <= 0 && (angleDiff <= aimTolerance || hasGlobalRange)) {
                if (t->type == TOWER_CRYO || t->type == TOWER_T4_CRYO_FREEZER) {
                    if (t->type == TOWER_CRYO) {
                        ApplyStatusEffect(target, STATUS_SLOW, dt * 1.5f, 0.6f);
                        ApplyStatusEffect(target, STATUS_BRITTLE, dt * 1.5f, 1.15f);
                    } else {
                        ApplyStatusEffect(target, STATUS_STUN, t->stats.fireRate, 1.0f);
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
    int particleCount = 15;
    float particleSpeed = 60.0f;
    float particleSize = 5.0f;
    bool gravity = (p->damageType == DMG_PHYSICAL);

    if (p->aoeRadius > 0) {
        particleCount = 60;
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

    // Chain lightning
    if (p->sourceType == TOWER_T4_TESLA_CHAIN && p->chainCount > 0) {
        int nextTargetId = -1;
        float minDist = 150.0f;
        for (int j = 0; j < MAX_ENEMIES; j++) {
            if (!game.enemies[j].active) continue;
            bool alreadyHit = false;
            for (int k = 0; k < p->hitCount; k++) {
                if (game.enemies[j].id == p->hitHistory[k]) { alreadyHit = true; break; }
            }
            if (alreadyHit) continue;
            float dist = Vector2Distance(p->position, game.enemies[j].position);
            if (dist < minDist) {
                minDist = dist;
                nextTargetId = game.enemies[j].id;
            }
        }
        if (nextTargetId != -1) {
            int idx = -1;
            for (int k = 0; k < MAX_PROJECTILES; k++) {
                if (!game.projectiles[k].active) { idx = k; break; }
            }
            if (idx != -1) {
                Projectile* chainP = &game.projectiles[idx];
                memcpy(chainP, p, sizeof(Projectile));
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

//----------------------------------------------------------------------------------
// Waves
//----------------------------------------------------------------------------------

void ManageWaves(float dt) {
    if (!game.waveActive) {
        game.waveTimer -= dt;
        if (game.waveTimer <= 0 || (IsKeyPressed(KEY_N) && game.state == GS_PLAYING)) {
            game.currentWave++;
            game.waveActive = true;
            game.enemiesToSpawn = 10 + (int)(game.currentWave * 3.5f);
            game.spawnTimer = 1.0f;
            AddFloatingTextFmt((Vector2){GAME_AREA_WIDTH/2, 50}, COLOR_ENERGY, true, "WAVE %d INCOMING!", game.currentWave);
            ScreenShake(3.0f, 0.5f);
        }
    } else {
        if (game.enemiesToSpawn > 0) {
            game.spawnTimer -= dt;
            if (game.spawnTimer <= 0) {
                EnemyType type = ENEMY_BASIC;
                int randVal = GetRandomValue(0, 100);
                if (game.currentWave >= 2 && randVal < 30) type = ENEMY_FAST;
                if (game.currentWave >= 4 && randVal >= 70 && randVal < 85) type = ENEMY_TANK;
                if (game.currentWave >= 6 && randVal >= 50 && randVal < 65) type = ENEMY_ETHEREAL;
                if (game.currentWave >= 8 && randVal >= 85 && randVal < 95) type = ENEMY_HEALER;
                if (game.currentWave >= 10 && randVal >= 40 && randVal < 50) type = ENEMY_SPAWNER;
                if (game.currentWave % 5 == 0 && game.enemiesToSpawn % 10 == 0)
                    SpawnEnemy(ENEMY_SPAWNER, game.map.waypoints[0]);
                if (game.currentWave % 10 == 0 && game.enemiesToSpawn == 1) {
                    type = ENEMY_BOSS;
                    ScreenShake(10.0f, 1.0f);
                }
                SpawnEnemy(type, game.map.waypoints[0]);
                game.enemiesToSpawn--;
                game.spawnTimer = fmaxf(0.3f, 1.4f - (float)game.currentWave * 0.04f);
            }
        } else {
            bool enemiesAlive = false;
            for (int i = 0; i < MAX_ENEMIES; i++) {
                if (game.enemies[i].active) { enemiesAlive = true; break; }
            }
            if (!enemiesAlive) {
                game.waveActive = false;
                game.waveTimer = WAVE_INTERVAL;
                int bonusGold = 50 + game.currentWave * 20;
                int bonusAether = 5 + game.currentWave / 3;
                game.gold += bonusGold;
                game.aether += bonusAether;
                AddFloatingText((Vector2){GAME_AREA_WIDTH/2, 100}, "Wave Complete! Bonus Resources!", COLOR_GOLD, false);
            }
        }
    }
}

//----------------------------------------------------------------------------------
// Drawing
//----------------------------------------------------------------------------------

void DrawGame(void) {
    BeginDrawing();
    ClearBackground(COLOR_BG);

    if (game.state == GS_TITLE) {
        DrawText("AETHERIUM VANGUARD", SCREEN_WIDTH/2 - MeasureText("AETHERIUM VANGUARD", 60)/2, SCREEN_HEIGHT/3 - 30, 60, COLOR_ENERGY);
        DrawText("PRESTIGE EDITION", SCREEN_WIDTH/2 - MeasureText("PRESTIGE EDITION", 40)/2, SCREEN_HEIGHT/3 + 40, 40, COLOR_AETHER_RES);
        float alpha = (sinf(game.globalTime * 2.0f) + 1.0f) / 2.0f;
        DrawText("Click or Press ENTER to Start", SCREEN_WIDTH/2 - MeasureText("Click or Press ENTER to Start", 30)/2, SCREEN_HEIGHT/2, 30, Fade(COLOR_TEXT_PRIMARY, alpha));
        DrawText("Controls: WASD (Move), Q (Dash), E (Burst), Space (Attack), 1-4/Click (Build), N (Next Wave), G (Grid)", 20, SCREEN_HEIGHT - 30, 18, COLOR_TEXT_MUTED);
    } else {
        BeginMode2D(game.camera);
        DrawMap();
        DrawEntities();
        DrawVFX();
        EndMode2D();

        DrawUI();

        if (game.state == GS_PAUSED) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.7f));
            DrawText("PAUSED (Press P)", SCREEN_WIDTH/2 - MeasureText("PAUSED (Press P)", 40)/2, SCREEN_HEIGHT/2 - 20, 40, WHITE);
        }

        if (game.state == GS_LEVEL_UP_HERO) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(COLOR_BG, 0.85f));
            Rectangle panel = { SCREEN_WIDTH/4, SCREEN_HEIGHT/4, SCREEN_WIDTH/2, SCREEN_HEIGHT/2 };
            DrawRectangleRounded(panel, 0.05f, 16, COLOR_UI_BG);
            DrawRectangleRoundedLinesEx(panel, 0.05f, 16, 2.0f, COLOR_AETHER_RES);

            DrawText("HERO LEVEL UP!", panel.x + 20, panel.y + 20, 30, COLOR_AETHER_RES);
            char heroLevelUpTitle[64];
            snprintf(heroLevelUpTitle, sizeof(heroLevelUpTitle), "Select a Skill (Skill Points: %d)", game.hero.skillPoints);
            DrawText(heroLevelUpTitle, panel.x + 20, panel.y + 60, 20, COLOR_TEXT_PRIMARY);

            int startY = (int)panel.y + 100;
            int buttonHeight = 50;
            int spacing = 15;
            const char* skillNames[] = {"Vigor (Attack+)", "Agility (Speed/Dash CD)", "Burst Mastery (AoE+)", "Leadership (Tower XP Aura)"};
            const char* skillDescs[] = {"Increases basic attack damage.", "Increases movement speed and reduces Dash cooldown.", "Increases Aether Burst damage and radius.", "Towers near the hero gain bonus XP."};

            for (int i = 0; i < NUM_HERO_SKILLS; i++) {
                Rectangle btnBounds = {panel.x + 20, startY + i * (buttonHeight + spacing), panel.width - 40, buttonHeight};
                char skillLabel[128];
                snprintf(skillLabel, sizeof(skillLabel), "%s (Level %d)", skillNames[i], game.hero.skills[i]);
                if (GuiButton(btnBounds, skillLabel, false, true)) {
                    if (game.hero.skillPoints > 0) {
                        game.hero.skills[i]++;
                        game.hero.skillPoints--;
                        ApplyHeroSkills();
                    }
                }
                SetTooltip(skillNames[i], skillDescs[i], btnBounds);
            }
        }

        if (game.state == GS_GAME_OVER) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(COLOR_DANGER, 0.3f));
            DrawText("GAME OVER", SCREEN_WIDTH/2 - MeasureText("GAME OVER", 60)/2, SCREEN_HEIGHT/2 - 50, 60, WHITE);
            char survived[64];
            snprintf(survived, sizeof(survived), "You survived %d waves.", game.currentWave);
            DrawText(survived, SCREEN_WIDTH/2 - MeasureText(survived, 30)/2, SCREEN_HEIGHT/2 + 20, 30, WHITE);
            DrawText("Press R to Restart", SCREEN_WIDTH/2 - MeasureText("Press R to Restart", 20)/2, SCREEN_HEIGHT/2 + 60, 20, WHITE);
        }

        if (game.tooltip.visible)
            DrawTooltip();
    }

    EndDrawing();
}

void DrawMap(void) {
    if (!game.mapRTBuilt) BuildStaticMapRT();
    Rectangle src = { 0, 0, (float)game.mapRT.texture.width, -(float)game.mapRT.texture.height };
    Rectangle dst = { 0, 0, GAME_AREA_WIDTH, SCREEN_HEIGHT };
    DrawTexturePro(game.mapRT.texture, src, dst, (Vector2){0,0}, 0.0f, WHITE);

    BeginBlendMode(BLEND_MULTIPLIED);
    DrawRectangle(0, 0, GAME_AREA_WIDTH, SCREEN_HEIGHT, game.environmentColor);
    EndBlendMode();

    if (game.showGrid) {
        for (int y = 0; y < MAP_HEIGHT; y++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                if (game.map.tiles[y][x] == 0) {
                    Rectangle tileRect = {x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE};
                    DrawRectangleLinesEx(tileRect, 1.0f, COLOR_GRID);
                }
            }
        }
    }
}

void DrawEntities(void) {
    DrawEnemies();
    DrawTowers();
    DrawHero();
    DrawProjectiles();
}

void DrawEnemies() {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!game.enemies[i].active) continue;
        Enemy* e = &game.enemies[i];

        // Base color by type
        Color baseColor;
        float size;
        switch (e->type) {
            case ENEMY_BASIC: baseColor = MAROON; size = 12.0f; break;
            case ENEMY_FAST: baseColor = ORANGE; size = 10.0f; break;
            case ENEMY_TANK: baseColor = DARKBROWN; size = 18.0f; break;
            case ENEMY_ETHEREAL: baseColor = Fade(COLOR_ENERGY, 0.8f); size = 14.0f; break;
            case ENEMY_HEALER: baseColor = LIME; size = 15.0f; break;
            case ENEMY_SPAWNER: baseColor = PURPLE; size = 20.0f; break;
            case ENEMY_MINION: baseColor = DARKGRAY; size = 8.0f; break;
            case ENEMY_BOSS: baseColor = RED; size = 28.0f; break;
            default: baseColor = GRAY; size = 10.0f;
        }

        // Apply visual state (computed in ProcessStatusEffects)
        Color drawColor = ColorTint(e->visualTint, game.environmentColor);

        if (e->type == ENEMY_HEALER)
            DrawPoly(e->position, 4, size, 45.0f, drawColor);
        else if (e->type == ENEMY_SPAWNER)
            DrawPoly(e->position, 5, size, 0.0f, drawColor);
        else
            DrawCircleV(e->position, size, drawColor);

        if (e->visualHasOutline)
            DrawCircleLines(e->position.x, e->position.y, size + 3.0f, e->visualOutlineColor);

        // Healer aura
        if (e->type == ENEMY_HEALER)
            DrawCircleV(e->position, 100.0f, Fade(LIME, 0.1f));

        // Stun text
        if (e->statusCount > 0) {
            for (int j = 0; j < e->statusCount; j++) {
                if (e->status[j].type == STATUS_STUN) {
                    DrawText("*STUN*", (int)e->position.x - 20, (int)e->position.y - size - 25, 16, YELLOW);
                    break;
                }
            }
        }

        // Health bar
        if (e->maxHp > 0 && e->hp < e->maxHp) {
            float hpPercent = Clamp(e->hp / e->maxHp, 0.0f, 1.0f);
            int barWidth = (size > 20) ? 60 : 40;
            Rectangle hpBar = {e->position.x - barWidth/2, e->position.y - size - 10, barWidth, 6};
            DrawRectangleRec(hpBar, COLOR_UI_BG);
            Color hpColor = (hpPercent < 0.3f) ? RED : (hpPercent < 0.6f) ? YELLOW : GREEN;
            DrawRectangle(hpBar.x, hpBar.y, (int)(hpBar.width * hpPercent), hpBar.height, hpColor);
            DrawRectangleLinesEx(hpBar, 1.0f, BLACK);
        }
    }
}

Color GetTowerColor(TowerType type) {
    switch (type) {
        case TOWER_PULSE: case TOWER_T4_PULSE_REPEATER: case TOWER_T4_PULSE_SNIPER: return COLOR_ENERGY;
        case TOWER_CANNON: case TOWER_T4_CANNON_MORTAR: case TOWER_T4_CANNON_VULCAN: return COLOR_PHYSICAL;
        case TOWER_CRYO: case TOWER_T4_CRYO_BLIZZARD: case TOWER_T4_CRYO_FREEZER: return COLOR_CRYO;
        case TOWER_TESLA: case TOWER_T4_TESLA_CHAIN: case TOWER_T4_TESLA_STORM: return COLOR_TESLA;
        default: return GRAY;
    }
}

void DrawTowers() {
    for (int i = 0; i < MAX_TOWERS; i++) {
        if (!game.towers[i].active) continue;
        Tower* t = &game.towers[i];
        Color color = GetTowerColor(t->type);
        Color baseColor = ColorTint(ColorBrightness(color, -0.6f), game.environmentColor);
        Color turretColor = ColorTint(color, game.environmentColor);

        // Base
        DrawRectangle(t->position.x - TILE_SIZE/2 + 4, t->position.y - TILE_SIZE/2 + 4, TILE_SIZE - 8, TILE_SIZE - 8, baseColor);

        // Specialized visuals
        if (t->type == TOWER_CRYO || t->type == TOWER_T4_CRYO_FREEZER) {
            Enemy* target = GetEnemyFromPair(t->targetIndex, t->targetEnemyId);
            if (target) {
                Vector2 dir = Vector2Subtract(target->position, t->position);
                float angleToTarget = atan2f(dir.y, dir.x) * RAD2DEG;
                float angleDiff = fabsf(fmodf(t->rotation - angleToTarget + 180.0f, 360.0f) - 180.0f);
                if (angleDiff <= GetAimToleranceDegrees(t->type)) {
                    float beamWidth = (t->type == TOWER_CRYO) ? 3.0f : 5.0f;
                    DrawLineEx(t->position, target->position, beamWidth, Fade(turretColor, 0.9f));
                    DrawLineEx(t->position, target->position, beamWidth + 6.0f, Fade(turretColor, 0.4f));
                }
            }
            DrawPoly(t->position, 6, 15, t->rotation, turretColor);
        } else if (t->type == TOWER_T4_CRYO_BLIZZARD) {
            float auraAlpha = 0.1f + (sinf(game.globalTime * 3.0f) + 1.0f) * 0.05f;
            DrawCircleV(t->position, t->stats.range, Fade(turretColor, auraAlpha));
            DrawCircleV(t->position, 18, turretColor);
        } else if (t->type == TOWER_TESLA || t->type == TOWER_T4_TESLA_CHAIN || t->type == TOWER_T4_TESLA_STORM) {
            DrawCircleV(t->position, 14, turretColor);
            if (GetRandomValue(0, 15) == 0) {
                float angle = GetRandomValue(0, 360) * DEG2RAD;
                Vector2 end = Vector2Add(t->position, Vector2Scale((Vector2){cosf(angle), sinf(angle)}, 22));
                DrawLineEx(t->position, end, 2.0f, Fade(turretColor, 0.8f));
            }
        } else {
            // Standard turret
            float length = 28.0f, width = 12.0f;
            Vector2 origin = { 8, 6 };
            if (t->type == TOWER_T4_PULSE_SNIPER) { length = 40.0f; width = 8.0f; }
            if (t->type == TOWER_CANNON) { length = 22.0f; width = 18.0f; }
            if (t->type == TOWER_T4_CANNON_MORTAR) { length = 25.0f; width = 20.0f; origin = (Vector2){12, 10}; }

            origin.x += t->visualRecoil;
            float rad = t->rotation * DEG2RAD;
            Vector2 forward = { cosf(rad), sinf(rad) };
            Vector2 perp = { -forward.y, forward.x };
            float barrelOffset = 4.0f;
            bool dual = (t->type == TOWER_T4_PULSE_REPEATER || t->type == TOWER_T4_CANNON_VULCAN);

            if (dual) {
                Vector2 pos1 = Vector2Add(t->position, Vector2Scale(perp, barrelOffset));
                Vector2 pos2 = Vector2Add(t->position, Vector2Scale(perp, -barrelOffset));
                Rectangle rect1 = { pos1.x, pos1.y, length, width };
                Rectangle rect2 = { pos2.x, pos2.y, length, width };
                DrawRectanglePro(rect1, origin, t->rotation, turretColor);
                DrawRectanglePro(rect2, origin, t->rotation, turretColor);
            } else {
                Rectangle rect = { t->position.x, t->position.y, length, width };
                DrawRectanglePro(rect, origin, t->rotation, turretColor);
            }
        }

        // Level indicator
        char lvlStr[8]; snprintf(lvlStr, sizeof(lvlStr), "L%d", t->stats.level);
        Color levelColor = (t->stats.level >= TOWER_BASE_MAX_LEVEL && t->type < TOWER_T4_PULSE_REPEATER) ? YELLOW : WHITE;
        DrawText(lvlStr, (int)t->position.x - TILE_SIZE/2 + 4, (int)t->position.y - TILE_SIZE/2 + 4, 16, levelColor);
    }
}

void DrawHero() {
    Hero* h = &game.hero;

    if (h->skills[SKILL_LEADERSHIP] > 0) {
        DrawCircleV(h->position, 150.0f, Fade(COLOR_XP, 0.15f));
    }

    Color heroColor = ColorTint(COLOR_AETHER_RES, game.environmentColor);
    Color heroOutline = ColorTint(PURPLE, game.environmentColor);
    DrawCircleV(h->position, 15, heroColor);
    DrawCircleLines(h->position.x, h->position.y, 15, heroOutline);
    char heroLvl[8]; snprintf(heroLvl, sizeof(heroLvl), "H%d", h->level);
    DrawText(heroLvl, (int)h->position.x - 10, (int)h->position.y - 8, 14, WHITE);
}

void DrawProjectiles() {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!game.projectiles[i].active) continue;
        Projectile* p = &game.projectiles[i];
        Color color = ColorTint(GetTowerColor(p->sourceType), game.environmentColor);
        float size = 4.0f;
        switch (p->sourceType) {
            case TOWER_CANNON: size = 8.0f; break;
            case TOWER_T4_CANNON_MORTAR: size = 12.0f; break;
            case TOWER_T4_PULSE_SNIPER: size = 6.0f; break;
            case TOWER_TESLA: case TOWER_T4_TESLA_CHAIN: size = 5.0f; break;
            default: break;
        }
        DrawCircleV(p->position, size, color);
        if (p->damageType == DMG_ENERGY)
            DrawCircleV(p->position, size + 5.0f, Fade(color, 0.4f));
    }
}

void DrawVFX(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!game.particles[i].active) continue;
        Particle* p = &game.particles[i];
        float progress = p->life / p->maxLife;
        Color color = ColorLerp(p->startColor, p->endColor, progress);
        float size = Lerp(p->startSize, p->endSize, progress);
        color = ColorTint(color, game.environmentColor);
        DrawCircleV(p->position, size, color);
    }

    for (int i = 0; i < MAX_FLOATING_TEXT; i++) {
        if (!game.floatingTexts[i].active) continue;
        FloatingText* ft = &game.floatingTexts[i];
        float alpha = Clamp(ft->lifetime / 0.5f, 0.0f, 1.0f);
        int fontSize = ft->critical ? 24 : 20;
        // Use DrawText for no rotation
        DrawText(ft->text, (int)ft->position.x + 1, (int)ft->position.y + 1, fontSize, Fade(BLACK, alpha));
        DrawText(ft->text, (int)ft->position.x, (int)ft->position.y, fontSize, Fade(ft->color, alpha));
    }
}

//----------------------------------------------------------------------------------
// UI
//----------------------------------------------------------------------------------

void DrawUI(void) {
    Vector2 mousePosScreen = GetMousePosition();
    Vector2 mousePosWorld = GetScreenToWorld2D(mousePosScreen, game.camera);

    BeginMode2D(game.camera);
    Vector2 centerPos = {0};
    float range = 0;
    Color color = WHITE;
    bool drawRange = false;

    if (game.placingTower != TOWER_NONE && mousePosScreen.x < GAME_AREA_WIDTH) {
        Vector2 tilePos = WorldToTile(mousePosWorld);
        centerPos = TileToWorldCenter((int)tilePos.x, (int)tilePos.y);
        Tower temp = {0};
        temp.type = game.placingTower;
        temp.stats.level = 1;
        ConfigureTowerStats(&temp);
        range = temp.stats.range;
        bool canBuild = IsTileBuildable((int)tilePos.x, (int)tilePos.y);
        color = canBuild ? GREEN : RED;
        drawRange = true;
        DrawRectangle(centerPos.x - TILE_SIZE/2, centerPos.y - TILE_SIZE/2, TILE_SIZE, TILE_SIZE, Fade(color, 0.5f));
    } else if (game.selectedTowerIndex != -1) {
        Tower* t = &game.towers[game.selectedTowerIndex];
        centerPos = t->position;
        range = t->stats.range;
        color = YELLOW;
        drawRange = true;
    }

    if (drawRange) {
        if (range > SCREEN_WIDTH * 2) {
            DrawText("GLOBAL RANGE", centerPos.x - 50, centerPos.y + TILE_SIZE/2 + 5, 20, color);
        } else {
            DrawCircleV(centerPos, range, Fade(color, 0.15f));
            DrawCircleLines(centerPos.x, centerPos.y, range, color);
        }
    }
    EndMode2D();

    // Sidebar
    DrawRectangle(GAME_AREA_WIDTH, 0, UI_WIDTH, SCREEN_HEIGHT, COLOR_UI_BG);
    DrawLine(GAME_AREA_WIDTH, 0, GAME_AREA_WIDTH, SCREEN_HEIGHT, COLOR_UI_ACCENT);

    DrawText("AETHERIUM VANGUARD", GAME_AREA_WIDTH + 10, 10, 24, COLOR_ENERGY);

    char livesStr[32]; snprintf(livesStr, sizeof(livesStr), "LIVES: %d", game.lives);
    DrawText(livesStr, GAME_AREA_WIDTH + 10, 40, 20, game.lives > 5 ? COLOR_TEXT_PRIMARY : COLOR_DANGER);
    char goldStr[32]; snprintf(goldStr, sizeof(goldStr), "GOLD: %d", game.gold);
    DrawText(goldStr, GAME_AREA_WIDTH + 150, 40, 20, COLOR_GOLD);
    char aetherStr[32]; snprintf(aetherStr, sizeof(aetherStr), "AETHER: %d", game.aether);
    DrawText(aetherStr, GAME_AREA_WIDTH + 150, 65, 20, COLOR_AETHER_RES);

    if (game.waveActive) {
        char waveStr[64]; snprintf(waveStr, sizeof(waveStr), "WAVE %d (Remaining: %d)", game.currentWave, game.enemiesToSpawn);
        DrawText(waveStr, GAME_AREA_WIDTH + 10, 95, 20, COLOR_TEXT_PRIMARY);
    } else {
        char nextStr[64]; snprintf(nextStr, sizeof(nextStr), "Next Wave: %.1fs (N)", game.waveTimer);
        DrawText(nextStr, GAME_AREA_WIDTH + 10, 95, 20, COLOR_TEXT_PRIMARY);
    }
    DrawLine(GAME_AREA_WIDTH, 125, SCREEN_WIDTH, 125, COLOR_UI_ACCENT);

    DrawHeroStatus();
    DrawLine(GAME_AREA_WIDTH, 260, SCREEN_WIDTH, 260, COLOR_UI_ACCENT);

    if (game.selectedTowerIndex != -1)
        DrawTowerInspector();
    else
        DrawBuildMenu();
}

void DrawHeroStatus() {
    DrawText("HERO STATUS", GAME_AREA_WIDTH + 10, 135, 20, COLOR_AETHER_RES);
    Hero* h = &game.hero;
    char levelStr[32]; snprintf(levelStr, sizeof(levelStr), "Level: %d", h->level);
    DrawText(levelStr, GAME_AREA_WIDTH + 10, 160, 18, COLOR_TEXT_PRIMARY);
    char atkStr[64]; snprintf(atkStr, sizeof(atkStr), "Attack: %d (Phys)", h->attackDamage);
    DrawText(atkStr, GAME_AREA_WIDTH + 130, 160, 18, COLOR_TEXT_PRIMARY);

    if (h->skillPoints > 0)
        DrawText("SKILL POINT AVAILABLE!", GAME_AREA_WIDTH + 10, 180, 18, YELLOW);

    Rectangle xpBar = {GAME_AREA_WIDTH + 10, 200, UI_WIDTH - 20, 20};
    float xpPercent = (h->xpToNextLevel > 0) ? (float)h->xp / h->xpToNextLevel : 0.0f;
    DrawRectangleRec(xpBar, COLOR_UI_ACCENT);
    DrawRectangle(xpBar.x, xpBar.y, (int)(xpBar.width * xpPercent), xpBar.height, COLOR_AETHER_RES);
    DrawRectangleLinesEx(xpBar, 1, BLACK);
    char xpStr[64]; snprintf(xpStr, sizeof(xpStr), "XP: %d / %d", h->xp, h->xpToNextLevel);
    DrawText(xpStr, xpBar.x + 10, xpBar.y + 2, 16, COLOR_TEXT_PRIMARY);

    Rectangle cdBarQ = {GAME_AREA_WIDTH + 10, 230, (UI_WIDTH - 30)/2, 20};
    float cdPercentQ = (h->dashCooldown > 0) ? 1.0f - Clamp(h->currentDashCooldown / h->dashCooldown, 0.0f, 1.0f) : 1.0f;
    DrawRectangleRec(cdBarQ, COLOR_UI_ACCENT);
    DrawRectangle(cdBarQ.x, cdBarQ.y, (int)(cdBarQ.width * cdPercentQ), cdBarQ.height, COLOR_AETHER_RES);
    DrawText("Dash (Q)", cdBarQ.x + 5, cdBarQ.y + 3, 14, COLOR_TEXT_PRIMARY);
    SetTooltip("Dash (Q)", "Rapid movement burst.", cdBarQ);

    Rectangle cdBarE = {GAME_AREA_WIDTH + 20 + (UI_WIDTH - 30)/2, 230, (UI_WIDTH - 30)/2, 20};
    float cdPercentE = (h->burstCooldown > 0) ? 1.0f - Clamp(h->currentBurstCooldown / h->burstCooldown, 0.0f, 1.0f) : 1.0f;
    DrawRectangleRec(cdBarE, COLOR_UI_ACCENT);
    DrawRectangle(cdBarE.x, cdBarE.y, (int)(cdBarE.width * cdPercentE), cdBarE.height, COLOR_ENERGY);
    DrawText("Burst (E)", cdBarE.x + 5, cdBarE.y + 3, 14, COLOR_TEXT_PRIMARY);
    SetTooltip("Aether Burst (E)", "AoE damage and energy weakness debuff.", cdBarE);
}

void DrawBuildMenu() {
    DrawText("BUILD MENU (1-4)", GAME_AREA_WIDTH + 10, 270, 20, COLOR_ENERGY);
    int startY = 300, buttonHeight = 65, spacing = 15;
    TowerType types[] = {TOWER_PULSE, TOWER_CANNON, TOWER_CRYO, TOWER_TESLA};

    for (int i = 0; i < 4; i++) {
        Rectangle btnBounds = {GAME_AREA_WIDTH + 10, startY + i * (buttonHeight + spacing), UI_WIDTH - 20, buttonHeight};
        int cost = GetTowerCost(types[i]);
        bool canAfford = game.gold >= cost;
        bool selected = game.placingTower == types[i];
        const char* name = GetTowerName(types[i]);
        const char* desc = GetTowerDescription(types[i]);
        char label[128]; snprintf(label, sizeof(label), "%s (%dG)", name, cost);
        if (GuiButton(btnBounds, label, selected, canAfford)) {
            if (canAfford) {
                game.placingTower = types[i];
                game.selectedTowerIndex = -1;
            } else {
                AddFloatingText((Vector2){GAME_AREA_WIDTH + 50, btnBounds.y}, "Not enough Gold!", COLOR_DANGER, false);
            }
        }
        SetTooltip(name, desc, btnBounds);
    }
}

void DrawTowerInspector() {
    if (game.selectedTowerIndex == -1 || !game.towers[game.selectedTowerIndex].active) {
        game.selectedTowerIndex = -1;
        return;
    }

    Tower* t = &game.towers[game.selectedTowerIndex];

    DrawText("TOWER INSPECTION", GAME_AREA_WIDTH + 10, 270, 20, YELLOW);

    char titleStr[128]; snprintf(titleStr, sizeof(titleStr), "%s (L%d)", GetTowerName(t->type), t->stats.level);
    DrawText(titleStr, GAME_AREA_WIDTH + 10, 300, 22, GetTowerColor(t->type));
    char killsStr[32]; snprintf(killsStr, sizeof(killsStr), "Kills: %d", t->kills);
    DrawText(killsStr, GAME_AREA_WIDTH + 200, 305, 18, COLOR_TEXT_PRIMARY);

    int currentY = 340;

    if (t->type >= TOWER_PULSE && t->type <= TOWER_TESLA) {
        DrawText("Experience:", GAME_AREA_WIDTH + 10, currentY, 18, COLOR_XP);
        currentY += 20;
        Rectangle xpBar = {GAME_AREA_WIDTH + 10, currentY, UI_WIDTH - 20, 20};
        if (t->stats.level < TOWER_BASE_MAX_LEVEL) {
            float xpPercent = (t->stats.xpToNextLevel > 0) ? (float)t->stats.xp / t->stats.xpToNextLevel : 0.0f;
            DrawRectangleRec(xpBar, COLOR_UI_ACCENT);
            DrawRectangle(xpBar.x, xpBar.y, (int)(xpBar.width * xpPercent), xpBar.height, COLOR_XP);
            DrawRectangleLinesEx(xpBar, 1, BLACK);
            char txp[64]; snprintf(txp, sizeof(txp), "%d / %d", t->stats.xp, t->stats.xpToNextLevel);
            DrawText(txp, xpBar.x + 10, xpBar.y + 2, 16, COLOR_TEXT_PRIMARY);
        } else {
            DrawRectangleRec(xpBar, COLOR_XP);
            DrawText("MAX LEVEL - READY FOR UPGRADE", xpBar.x + 10, xpBar.y + 2, 16, BLACK);
        }
        currentY += 30;
    } else {
        DrawText("Specialized Tower (Max Tier)", GAME_AREA_WIDTH + 10, currentY, 18, COLOR_XP);
        currentY += 30;
    }

    currentY += 10;
    DrawText("Stats:", GAME_AREA_WIDTH + 10, currentY, 20, YELLOW);
    currentY += 25;

    const char* dmgType = t->damageType == DMG_ENERGY ? "Energy" : (t->damageType == DMG_PHYSICAL ? "Physical" : "True");
    Color statColor = GetTowerColor(t->type);
    char dmgStr[64]; snprintf(dmgStr, sizeof(dmgStr), "Damage: %.1f (%s)", t->stats.damage, dmgType);
    DrawText(dmgStr, GAME_AREA_WIDTH + 10, currentY, 18, statColor);
    currentY += 25;

    const char* rangeText = (t->stats.range > SCREEN_WIDTH * 2) ? "Global" : NULL;
    char rangeBuf[32]; if (!rangeText) { snprintf(rangeBuf, sizeof(rangeBuf), "%.0f", t->stats.range); rangeText = rangeBuf; }
    char rangeStr[64]; snprintf(rangeStr, sizeof(rangeStr), "Range: %s", rangeText);
    DrawText(rangeStr, GAME_AREA_WIDTH + 10, currentY, 18, COLOR_TEXT_PRIMARY);
    currentY += 25;

    if (t->type == TOWER_CRYO || t->type == TOWER_T4_CRYO_FREEZER) {
        DrawText("Fire Rate: Continuous (DPS)", GAME_AREA_WIDTH + 10, currentY, 18, COLOR_TEXT_PRIMARY);
    } else if (t->type == TOWER_T4_CRYO_BLIZZARD) {
        char slowStr[64]; snprintf(slowStr, sizeof(slowStr), "Slow Aura: %.0f%%", t->stats.fireRate * 100.0f);
        DrawText(slowStr, GAME_AREA_WIDTH + 10, currentY, 18, COLOR_TEXT_PRIMARY);
    } else {
        char rateStr[64]; snprintf(rateStr, sizeof(rateStr), "Fire Rate: %.2f/s", t->stats.fireRate);
        DrawText(rateStr, GAME_AREA_WIDTH + 10, currentY, 18, COLOR_TEXT_PRIMARY);
    }
    currentY += 35;

    DrawLine(GAME_AREA_WIDTH, currentY, SCREEN_WIDTH, currentY, COLOR_UI_ACCENT);
    currentY += 10;

    if (t->type != TOWER_T4_CRYO_BLIZZARD) {
        DrawText("Targeting Mode:", GAME_AREA_WIDTH + 10, currentY, 20, YELLOW);
        currentY += 25;
        Rectangle btnTarget = {GAME_AREA_WIDTH + 10, currentY, UI_WIDTH - 20, 40};
        const char* modeText = "Unknown";
        switch (t->targetingMode) {
            case TARGET_FIRST: modeText = "First (Default)"; break;
            case TARGET_CLOSEST: modeText = "Closest"; break;
            case TARGET_STRONGEST: modeText = "Strongest (HP)"; break;
            case TARGET_WEAKEST: modeText = "Weakest (HP)"; break;
        }
        if (GuiButton(btnTarget, modeText, false, true)) {
            t->targetingMode = (t->targetingMode + 1) % NUM_TARGETING_MODES;
        }
        SetTooltip("Targeting Mode", "Change how the tower prioritizes enemies.", btnTarget);
        currentY += 50;
    }

    if (t->stats.level >= TOWER_BASE_MAX_LEVEL && t->type >= TOWER_PULSE && t->type <= TOWER_TESLA) {
        DrawTowerUpgradePaths(t);
    }

    Rectangle btnSell = {GAME_AREA_WIDTH + 10, SCREEN_HEIGHT - 50, UI_WIDTH - 20, 40};
    int sellValue = (int)(t->totalCost * 0.6f);
    char sellLabel[64]; snprintf(sellLabel, sizeof(sellLabel), "Sell Tower (%dG)", sellValue);
    if (GuiButton(btnSell, sellLabel, false, true)) {
        SellTower(game.selectedTowerIndex);
    }
    SetTooltip("Sell", "Sell the tower for a partial refund.", btnSell);
}

void DrawTowerUpgradePaths(Tower* t) {
    DrawText("T4 UPGRADE PATHS (Requires Aether)", GAME_AREA_WIDTH + 10, 600, 20, COLOR_AETHER_RES);

    TowerType path1 = TOWER_NONE, path2 = TOWER_NONE;
    switch (t->type) {
        case TOWER_PULSE: path1 = TOWER_T4_PULSE_REPEATER; path2 = TOWER_T4_PULSE_SNIPER; break;
        case TOWER_CANNON: path1 = TOWER_T4_CANNON_MORTAR; path2 = TOWER_T4_CANNON_VULCAN; break;
        case TOWER_CRYO: path1 = TOWER_T4_CRYO_BLIZZARD; path2 = TOWER_T4_CRYO_FREEZER; break;
        case TOWER_TESLA: path1 = TOWER_T4_TESLA_CHAIN; path2 = TOWER_T4_TESLA_STORM; break;
        default: return;
    }

    int startY = 630, buttonHeight = 70;
    TowerType paths[] = {path1, path2};

    for (int i = 0; i < 2; i++) {
        Rectangle btnBounds = {GAME_AREA_WIDTH + 10, startY + i * (buttonHeight + 15), UI_WIDTH - 20, buttonHeight};
        int costGold = GetTowerCost(paths[i]);
        int costAether = GetTowerAetherCost(paths[i]);
        bool canAfford = (game.gold >= costGold) && (game.aether >= costAether);
        const char* name = GetTowerName(paths[i]);
        const char* desc = GetTowerDescription(paths[i]);
        char upLabel[128]; snprintf(upLabel, sizeof(upLabel), "%s (%dG, %dA)", name, costGold, costAether);
        if (GuiButton(btnBounds, upLabel, false, canAfford)) {
            if (canAfford) {
                game.gold -= costGold;
                game.aether -= costAether;
                if (UpgradeTower(t, paths[i])) {
                    t->totalCost += costGold;
                    AddFloatingText(t->position, "UPGRADED!", COLOR_AETHER_RES, true);
                    ScreenShake(3.0f, 0.3f);
                }
            } else {
                AddFloatingText((Vector2){GAME_AREA_WIDTH + 50, btnBounds.y}, "Cannot Afford!", COLOR_DANGER, false);
            }
        }
        SetTooltip(name, desc, btnBounds);
    }
}

void DrawTooltip() {
    if (!game.tooltip.visible) return;
    Vector2 mousePos = GetMousePosition();
    int padding = 10, titleSize = 20, textSize = 16, maxWidth = 300;

    int lines = 1;
    for (const char* p = game.tooltip.description; *p; p++)
        if (*p == '\n') lines++;
    int textHeight = lines * (textSize + 5);
    int titleHeight = titleSize + 5;
    int width = MeasureText(game.tooltip.title, titleSize);
    int descWidth = MeasureText(game.tooltip.description, textSize);
    if (descWidth > width) width = descWidth;
    if (width > maxWidth) width = maxWidth;

    Rectangle tooltipRect = {
        mousePos.x + 15, mousePos.y + 15,
        width + padding * 2,
        titleHeight + textHeight + padding * 3
    };
    if (tooltipRect.x + tooltipRect.width > SCREEN_WIDTH)
        tooltipRect.x = SCREEN_WIDTH - tooltipRect.width - 5;
    if (tooltipRect.y + tooltipRect.height > SCREEN_HEIGHT)
        tooltipRect.y = mousePos.y - tooltipRect.height - 15;

    DrawRectangleRec(tooltipRect, Fade(COLOR_BG, 0.9f));
    DrawRectangleLinesEx(tooltipRect, 1.0f, COLOR_UI_ACCENT);

    DrawText(game.tooltip.title, tooltipRect.x + padding, tooltipRect.y + padding, titleSize, COLOR_ENERGY);
    DrawLine(tooltipRect.x + padding, tooltipRect.y + padding + titleHeight + 5,
             tooltipRect.x + width + padding, tooltipRect.y + padding + titleHeight + 5, COLOR_UI_ACCENT);
    DrawText(game.tooltip.description, tooltipRect.x + padding, tooltipRect.y + padding * 2 + titleHeight, textSize, COLOR_TEXT_PRIMARY);
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
                enemy->visualTint = ColorAlphaBlend(enemy->visualTint, COLOR_CRYO, 0.5f);
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
// VFX
//----------------------------------------------------------------------------------

void UpdateVFX(float dt) {
    for (int i = 0; i < MAX_FLOATING_TEXT; i++) {
        if (game.floatingTexts[i].active) {
            game.floatingTexts[i].lifetime -= dt;
            game.floatingTexts[i].position.y += game.floatingTexts[i].velocityY * dt;
            game.floatingTexts[i].velocityY = Lerp(game.floatingTexts[i].velocityY, 0.0f, 3.0f * dt);
            if (game.floatingTexts[i].lifetime <= 0)
                game.floatingTexts[i].active = false;
        }
    }

    float clamped = Clamp(dt, 0.0f, 0.1f);
    float dampingGravityFactor = powf(0.99f, clamped * 60.0f);
    float dampingNoGravFactor = powf(0.95f, clamped * 60.0f);

    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!game.particles[i].active) continue;
        Particle* p = &game.particles[i];
        p->life += dt;
        if (p->life >= p->maxLife) { p->active = false; continue; }
        if (p->gravity) p->velocity.y += 200.0f * dt;
        p->position = Vector2Add(p->position, Vector2Scale(p->velocity, dt));
        float df = p->gravity ? dampingGravityFactor : dampingNoGravFactor;
        p->velocity = Vector2Scale(p->velocity, df);
    }
}

void SpawnParticles(Vector2 position, int count, Color startColor, Color endColor, float speed, float sizeStart, float sizeEnd, bool gravity) {
    for (int i = 0; i < count; i++) {
        // Free-list allocation
        int index = -1;
        for (int j = game.nextFreeParticle; j < MAX_PARTICLES; j++) {
            if (!game.particles[j].active) { index = j; break; }
        }
        if (index == -1) {
            for (int j = 0; j < game.nextFreeParticle; j++) {
                if (!game.particles[j].active) { index = j; break; }
            }
        }
        if (index == -1) return;
        game.nextFreeParticle = (index + 1) % MAX_PARTICLES;

        Particle* p = &game.particles[index];
        p->active = true;
        p->position = position;
        p->startColor = startColor;
        p->endColor = endColor;
        p->life = 0;
        p->maxLife = GetRandomValue(5, 20) / 10.0f;
        p->startSize = sizeStart * (GetRandomValue(8, 12) / 10.0f);
        p->endSize = sizeEnd;
        p->gravity = gravity;
        float angle = GetRandomValue(0, 360) * DEG2RAD;
        float magnitude = GetRandomValue(10, (int)speed);
        p->velocity = (Vector2){ cosf(angle) * magnitude, sinf(angle) * magnitude };
        if (gravity && p->velocity.y > 0) p->velocity.y *= -0.5f;
    }
}

void AddFloatingText(Vector2 position, const char* text, Color color, bool critical) {
    int index = -1;
    for (int i = game.nextFreeFloatingText; i < MAX_FLOATING_TEXT; i++) {
        if (!game.floatingTexts[i].active) { index = i; break; }
    }
    if (index == -1) {
        for (int i = 0; i < game.nextFreeFloatingText; i++) {
            if (!game.floatingTexts[i].active) { index = i; break; }
        }
    }
    if (index == -1) return;
    game.nextFreeFloatingText = (index + 1) % MAX_FLOATING_TEXT;

    FloatingText* ft = &game.floatingTexts[index];
    ft->active = true;
    ft->position = (Vector2){position.x + GetRandomValue(-5, 5), position.y + GetRandomValue(-5, 5)};
    strncpy(ft->text, text, 31);
    ft->text[31] = '\0';
    ft->color = color;
    ft->lifetime = critical ? 2.0f : 1.5f;
    ft->critical = critical;
    ft->velocityY = critical ? -80.0f : -40.0f;
}

void AddFloatingTextFmt(Vector2 position, Color color, bool critical, const char* fmt, ...) {
    int index = -1;
    for (int i = game.nextFreeFloatingText; i < MAX_FLOATING_TEXT; i++) {
        if (!game.floatingTexts[i].active) { index = i; break; }
    }
    if (index == -1) {
        for (int i = 0; i < game.nextFreeFloatingText; i++) {
            if (!game.floatingTexts[i].active) { index = i; break; }
        }
    }
    if (index == -1) return;
    game.nextFreeFloatingText = (index + 1) % MAX_FLOATING_TEXT;

    FloatingText* ft = &game.floatingTexts[index];
    ft->active = true;
    ft->position = (Vector2){position.x + GetRandomValue(-5, 5), position.y + GetRandomValue(-5, 5)};
    ft->color = color;
    ft->lifetime = critical ? 2.0f : 1.5f;
    ft->critical = critical;
    ft->velocityY = critical ? -80.0f : -40.0f;

    va_list args;
    va_start(args, fmt);
    vsnprintf(ft->text, sizeof(ft->text), fmt, args);
    va_end(args);
    ft->text[sizeof(ft->text) - 1] = '\0';
}

void ScreenShake(float intensity, float duration) {
    if (intensity > game.screenShakeIntensity || game.screenShakeTime <= 0) {
        game.screenShakeIntensity = intensity;
        game.screenShakeTime = duration;
        game.screenShakeDuration = duration;
    }
}

//----------------------------------------------------------------------------------
// Utility
//----------------------------------------------------------------------------------

Vector2 WorldToTile(Vector2 worldPos) {
    return (Vector2){ floorf(worldPos.x / TILE_SIZE), floorf(worldPos.y / TILE_SIZE) };
}

Vector2 TileToWorldCenter(int x, int y) {
    return (Vector2){ x * TILE_SIZE + TILE_SIZE/2.0f, y * TILE_SIZE + TILE_SIZE/2.0f };
}

Vector2 ClosestPointOnSegment(Vector2 p, Vector2 a, Vector2 b) {
    Vector2 ab = Vector2Subtract(b, a);
    float lengthSqr = Vector2LengthSqr(ab);
    if (lengthSqr == 0.0f) return a;
    float t = Vector2DotProduct(Vector2Subtract(p, a), ab) / lengthSqr;
    t = Clamp(t, 0.0f, 1.0f);
    return Vector2Add(a, Vector2Scale(ab, t));
}

bool IsTileBuildable(int x, int y) {
    if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) return false;
    if (game.map.tiles[y][x] != 0) return false;
    return !game.occupied[y][x];
}

Enemy* GetEnemyById(int id) {
    if (id <= 0) return NULL;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (game.enemies[i].active && game.enemies[i].id == id)
            return &game.enemies[i];
    }
    return NULL;
}

int GetEnemyIndexById(int id) {
    if (id <= 0) return -1;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (game.enemies[i].active && game.enemies[i].id == id)
            return i;
    }
    return -1;
}

float LerpAngle(float start, float end, float amount) {
    float difference = fmodf(end - start + 360.0f, 360.0f);
    if (difference > 180.0f) difference -= 360.0f;
    return start + difference * amount;
}

static inline Enemy* GetEnemyFromPair(int index, int id) {
    if (index >= 0 && index < MAX_ENEMIES) {
        Enemy* e = &game.enemies[index];
        if (e->active && e->id == id) return e;
    }
    return NULL;
}

int GetTowerCost(TowerType type) {
    switch (type) {
        case TOWER_PULSE: return 100;
        case TOWER_CANNON: return 250;
        case TOWER_CRYO: return 200;
        case TOWER_TESLA: return 400;
        case TOWER_T4_PULSE_REPEATER: return 800;
        case TOWER_T4_PULSE_SNIPER: return 1000;
        case TOWER_T4_CANNON_MORTAR: return 1500;
        case TOWER_T4_CANNON_VULCAN: return 900;
        case TOWER_T4_CRYO_BLIZZARD: return 700;
        case TOWER_T4_CRYO_FREEZER: return 850;
        case TOWER_T4_TESLA_CHAIN: return 1100;
        case TOWER_T4_TESLA_STORM: return 1300;
        default: return 99999;
    }
}

int GetTowerAetherCost(TowerType type) {
    if (type >= TOWER_T4_PULSE_REPEATER)
        return 50 + (GetTowerCost(type) / 20);
    return 0;
}

const char* GetTowerName(TowerType type) {
    switch (type) {
        case TOWER_PULSE: return "Pulse Emitter [1]";
        case TOWER_CANNON: return "Aether Cannon [2]";
        case TOWER_CRYO: return "Cryo Beam [3]";
        case TOWER_TESLA: return "Tesla Coil [4]";
        case TOWER_T4_PULSE_REPEATER: return "T4: Pulse Repeater";
        case TOWER_T4_PULSE_SNIPER: return "T4: Marksman Laser";
        case TOWER_T4_CANNON_MORTAR: return "T4: Orbital Mortar";
        case TOWER_T4_CANNON_VULCAN: return "T4: Vulcan Gatling";
        case TOWER_T4_CRYO_BLIZZARD: return "T4: Blizzard Field";
        case TOWER_T4_CRYO_FREEZER: return "T4: Deep Freezer";
        case TOWER_T4_TESLA_CHAIN: return "T4: Chain Lightning";
        case TOWER_T4_TESLA_STORM: return "T4: Storm Caller";
        default: return "Unknown";
    }
}

const char* GetTowerDescription(TowerType type) {
    switch (type) {
        case TOWER_PULSE: return "Rapid-fire energy weapon. Effective against most targets.";
        case TOWER_CANNON: return "Fires explosive physical shells. Deals AoE damage.\nSYNERGY: Applies Burn and Melted Armor.";
        case TOWER_CRYO: return "Continuous energy beam that slows a target.\nSYNERGY: Applies Brittle (Increased Physical Dmg Taken).";
        case TOWER_TESLA: return "High-damage energy attacks with a chance to stun the target.";
        case TOWER_T4_PULSE_REPEATER: return "Upgraded Pulse Emitter with extreme fire rate. Overwhelms targets with energy.";
        case TOWER_T4_PULSE_SNIPER: return "Long-range, slow-firing laser that deals massive energy damage.";
        case TOWER_T4_CANNON_MORTAR: return "Global range artillery. Fires slow-moving shells with a massive explosion radius.\nUses predictive ballistic targeting.";
        case TOWER_T4_CANNON_VULCAN: return "High-speed physical Gatling gun. Shreds armor and applies Burn rapidly.";
        case TOWER_T4_CRYO_BLIZZARD: return "Creates a persistent slowing field around the tower, affecting all enemies within.";
        case TOWER_T4_CRYO_FREEZER: return "Upgraded Cryo Beam that completely freezes (stuns) the target for a duration.";
        case TOWER_T4_TESLA_CHAIN: return "Lightning attack that chains to multiple nearby enemies.";
        case TOWER_T4_TESLA_STORM: return "High damage Tesla Coil with increased stun chance and damage.";
        default: return "No description available.";
    }
}

bool GuiButton(Rectangle bounds, const char* text, bool selected, bool enabled) {
    Vector2 mousePos = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mousePos, bounds);
    bool clicked = false;

    Color bgColor = COLOR_UI_ACCENT;
    Color textColor = COLOR_TEXT_PRIMARY;
    Color borderColor = COLOR_GRID;

    if (!enabled) {
        bgColor = ColorBrightness(COLOR_UI_ACCENT, -0.4f);
        textColor = COLOR_TEXT_MUTED;
    } else if (selected) {
        bgColor = COLOR_ENERGY;
        textColor = BLACK;
        borderColor = YELLOW;
    } else if (hovered) {
        bgColor = ColorBrightness(COLOR_UI_ACCENT, 0.2f);
        borderColor = YELLOW;
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            clicked = true;
    }

    DrawRectangleRounded(bounds, 0.1f, 8, bgColor);
    DrawRectangleRoundedLinesEx(bounds, 0.1f, 8, 2.0f, borderColor);

    int textSize = 18;
    int textWidth = MeasureText(text, textSize);
    DrawText(text, bounds.x + bounds.width/2 - textWidth/2, bounds.y + bounds.height/2 - textSize/2, textSize, textColor);

    return clicked && enabled;
}

void SetTooltip(const char* title, const char* description, Rectangle bounds) {
    if (CheckCollisionPointRec(GetMousePosition(), bounds)) {
        game.tooltip.title = title;
        game.tooltip.description = description;
        game.tooltip.bounds = bounds;
        game.tooltip.visible = true;
    }
}