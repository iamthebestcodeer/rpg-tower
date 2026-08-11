#include "game.h"
#include <time.h>

GameData game = {0};

// High-resolution monotonic clock for the benchmark (raylib's GetTime() proved
// unreliable for stable per-frame timing on this build).
double NowMs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec * 1e-6;
}

//----------------------------------------------------------------------------------
// Benchmark harness (--bench [--frames N])
//
// Runs a heavy, deterministic battle scene with the FPS cap removed, measures
// per-frame wall time over N frames (after a short warmup), prints a summary
// to stdout and exits. No effect on normal gameplay. Used to verify
// performance work with a stable, repeatable workload.
//----------------------------------------------------------------------------------
#define BENCH_DEFAULT_FRAMES 400
#define BENCH_MAX_FRAMES     1000000 // ceiling on --frames: bounds the sample buffer and the run
#define BENCH_WARMUP_FRAMES  30

static bool g_benchMode = false;
static bool g_benchReady = false;
static int g_benchTotalFrames = BENCH_DEFAULT_FRAMES;
static int g_benchFrame = 0;
static double g_benchLastTime = 0.0;
static double g_benchLastUpdateMs = 0.0;
static double g_benchLastDrawMs = 0.0;
static double g_benchLoopMs = 0.0; // loop iteration, timed externally with NowMs
static double g_benchSumMs = 0.0;
static double g_benchMinMs = 1e9;
static double g_benchMaxMs = 0.0;
static double* g_benchSamples = NULL; // per-frame times for median/percentiles
static double g_benchUpdateMs = 0.0;
static double g_benchDrawMs = 0.0;
static long g_sumParticles = 0, g_sumEnemies = 0, g_sumProjectiles = 0, g_sumTexts = 0;

// Parses the --frames argument into a frame count. Returns -1 when the value
// is not a positive integer within [1, BENCH_MAX_FRAMES] so main() can print
// the usage line and abort - an unchecked count would corrupt the sample
// buffer (zero/negative indexing) or the reported statistics (divide by zero).
static int ParseBenchFrames(const char* raw) {
    char* end = NULL;
    long count = strtol(raw, &end, 10);
    if (end == raw || *end != '\0' || count <= 0 || count > BENCH_MAX_FRAMES)
        return -1;
    return (int)count;
}

static void PrintBenchUsage(const char* program) {
    fprintf(stderr, "Usage: %s [--bench [--frames <positive integer>]]\n", program);
}

static void SetupBenchmarkScene(void) {
    // Deterministic heavy scene: fill the map with towers, then drop a large
    // mixed wave so towers fire, projectiles fly, particles and floating text
    // churn at maximum pressure.
    ResetGame();
    game.state = GS_PLAYING;
    game.lives = 100000;
    game.gold = 99999;
    game.hero.xpToNextLevel = 2000000000; // hero must not level mid-bench (state switch)

    const TowerType cycle[4] = { TOWER_PULSE, TOWER_CANNON, TOWER_CRYO, TOWER_TESLA };
    int placed = 0;
    for (int y = 0; y < MAP_HEIGHT && placed < MAX_TOWERS; y++) {
        for (int x = 0; x < MAP_WIDTH && placed < MAX_TOWERS; x++) {
            if (IsTileBuildable(x, y) && PlaceTower(x, y, cycle[placed % 4]))
                placed++;
        }
    }

    game.waveActive = true;
    game.currentWave = 25;
    game.enemiesToSpawn = 0; // manual spawn below; no scheduled extras
    const EnemyType mix[8] = { ENEMY_BASIC, ENEMY_BASIC, ENEMY_FAST, ENEMY_TANK,
                               ENEMY_ETHEREAL, ENEMY_HEALER, ENEMY_SPAWNER, ENEMY_BOSS };
    for (int i = 0; i < 200; i++)
        SpawnEnemy(mix[i % 8], game.map.waypoints[0]);
}

static bool BenchmarkTick(void) {
    if (!g_benchReady) {
        g_benchReady = true;
        g_benchLastTime = NowMs();
        return false;
    }
    double now = NowMs();
    double ms = now - g_benchLastTime;
    g_benchLastTime = now;
    g_benchFrame++;

    if (g_benchFrame > BENCH_WARMUP_FRAMES && g_benchFrame <= BENCH_WARMUP_FRAMES + g_benchTotalFrames) {
        g_benchSumMs += ms;
        if (ms < g_benchMinMs) g_benchMinMs = ms;
        if (ms > g_benchMaxMs) g_benchMaxMs = ms;
        g_benchSamples[g_benchFrame - BENCH_WARMUP_FRAMES - 1] = ms;
    }

    // Per-phase timing for profiling (always tracked, printed at the end)
    g_benchUpdateMs += g_benchLastUpdateMs;
    g_benchDrawMs += g_benchLastDrawMs;

    if (g_benchFrame > BENCH_WARMUP_FRAMES) {
        int pe = 0, en = 0, pr = 0, ft = 0;
        for (int i = 0; i < MAX_PARTICLES; i++) if (game.particles[i].active) pe++;
        for (int i = 0; i < MAX_ENEMIES; i++) if (game.enemies[i].active) en++;
        for (int i = 0; i < MAX_PROJECTILES; i++) if (game.projectiles[i].active) pr++;
        for (int i = 0; i < MAX_FLOATING_TEXT; i++) if (game.floatingTexts[i].active) ft++;
        g_sumParticles += pe; g_sumEnemies += en; g_sumProjectiles += pr; g_sumTexts += ft;
    }

    if (g_benchFrame >= BENCH_WARMUP_FRAMES + g_benchTotalFrames) {
        double avgMs = g_benchSumMs / g_benchTotalFrames;
        // Median + P90 (robust against the machine's background noise spikes)
        for (int i = 1; i < g_benchTotalFrames; i++) {
            double key = g_benchSamples[i];
            int j = i - 1;
            while (j >= 0 && g_benchSamples[j] > key) { g_benchSamples[j + 1] = g_benchSamples[j]; j--; }
            g_benchSamples[j + 1] = key;
        }
        double med = g_benchSamples[g_benchTotalFrames / 2];
        double p90 = g_benchSamples[(int)(g_benchTotalFrames * 0.9)];
        printf("\nBENCHMARK frames=%d avg=%.3fms (%.1f fps) med=%.3fms (%.1f fps) p90=%.3fms min=%.3fms max=%.3fms\n",
               g_benchTotalFrames, avgMs, 1000.0 / avgMs, med, 1000.0 / med, p90, g_benchMinMs, g_benchMaxMs);
        printf("BENCHMARK loop=%.3fms update=%.3fms draw=%.3fms (other=%.3fms)\n",
               g_benchLoopMs / g_benchTotalFrames,
               g_benchUpdateMs / g_benchTotalFrames, g_benchDrawMs / g_benchTotalFrames,
               (g_benchLoopMs - g_benchUpdateMs - g_benchDrawMs) / g_benchTotalFrames);
        printf("BENCHMARK load particles=%.0f enemies=%.0f projectiles=%.0f texts=%.0f\n",
               (double)g_sumParticles / g_benchTotalFrames, (double)g_sumEnemies / g_benchTotalFrames,
               (double)g_sumProjectiles / g_benchTotalFrames, (double)g_sumTexts / g_benchTotalFrames);
        fflush(stdout);
        return true;
    }
    return false;
}



//----------------------------------------------------------------------------------
// Main
//----------------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--bench") == 0) {
            g_benchMode = true;
        } else if (strcmp(argv[i], "--frames") == 0) {
            if (i + 1 >= argc) {
                PrintBenchUsage(argv[0]);
                return 1;
            }
            int frames = ParseBenchFrames(argv[++i]);
            if (frames < 0) {
                PrintBenchUsage(argv[0]);
                return 1;
            }
            g_benchTotalFrames = frames;
        }
    }

    // Resource note: MSAA 4x was the single biggest GPU cost in this game - it
    // multisamples the whole 1280x800 framebuffer at 4x fill on every frame at
    // composite time, for little visible gain on flat-color shapes. Re-enable
    // with SetConfigFlags(FLAG_MSAA_4X_HINT) before InitWindow() if smoother
    // edges are preferred over lower GPU usage.
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Aetherium Vanguard");
    if (g_benchMode) {
        SetTargetFPS(0);             // uncapped so real frame time is measurable
        SetRandomSeed(1234);         // deterministic bench
    } else {
        SetTargetFPS(60); // Cap the render loop; without it raylib spins at max FPS and pegs a CPU core
        SetRandomSeed((unsigned int)time(NULL));
    }

    InitGame();
    if (g_benchMode) {
        SetupBenchmarkScene();
        g_benchSamples = (double*)malloc((size_t)g_benchTotalFrames * sizeof(double));
        if (!g_benchSamples) {
            fprintf(stderr, "BENCHMARK: failed to allocate %d sample slots\n", g_benchTotalFrames);
            CloseWindow();
            return 1;
        }
    }

    while (!WindowShouldClose()) {
        double iterStart = NowMs();
        float dt = GetFrameTime();
        if (dt > 0.1f) dt = 0.1f;
        game.globalTime += dt;

        if (g_benchMode) {
            dt = 1.0f / 60.0f; // fixed timestep for deterministic benchmark
            double t0 = NowMs();
            UpdateGame(dt);
            double t1 = NowMs();
            DrawGame();
            double t2 = NowMs();
            g_benchLastUpdateMs = t1 - t0;
            g_benchLastDrawMs = t2 - t1;
            if (BenchmarkTick()) break;
            g_benchLoopMs += NowMs() - iterStart;
        } else {
            UpdateGame(dt);
            DrawGame();
        }
    }

    if (game.mapRTBuilt) UnloadRenderTexture(game.mapRT);
    UnloadVFX();
    CloseWindow();
    if (g_benchSamples) free(g_benchSamples);
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
    InitVFX();
}

void ResetGame(void) {
    memset(game.towers, 0, sizeof(game.towers));
    memset(game.enemies, 0, sizeof(game.enemies));
    memset(game.projectiles, 0, sizeof(game.projectiles));
    memset(game.particles, 0, sizeof(game.particles));
    memset(game.floatingTexts, 0, sizeof(game.floatingTexts));
    memset(game.occupied, 0, sizeof(game.occupied));

    game.lives = STARTING_LIVES;
    game.gold = STARTING_GOLD;
    game.aether = STARTING_AETHER;
    game.currentWave = 0;
    game.waveTimer = 10.0f;
    game.waveActive = false;
    game.placingTower = TOWER_NONE;
    game.selectedTowerIndex = -1;
    game.enemyIdCounter = 1;
    game.dayNightCycle = 0.0f;
    game.screenShakeDuration = 0.0f;
    game.screenShakeTime = 0.0f;
    game.screenShakeIntensity = 0.0f;
    game.enemiesToSpawn = 0;
    game.spawnTimer = 0.0f;
    game.tooltip.visible = false;
    game.tooltip.title = NULL;
    game.tooltip.description = NULL;
    // Match what the first UpdateEnvironment computes at dayNightCycle = 0 (full
    // daylight), so the first gameplay draw after starting/restarting isn't black
    // because UpdateEnvironment is skipped while the state is GS_TITLE.
    game.environmentColor = WHITE;

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
    game.mapRTBuilt = IsRenderTextureValid(game.mapRT);
}
