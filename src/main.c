#include "game.h"

GameData game = {0};

//----------------------------------------------------------------------------------
// Main
//----------------------------------------------------------------------------------

int main(void) {
    // Resource note: MSAA 4x was the single biggest GPU cost in this game - it
    // multisamples the whole 1280x800 framebuffer at 4x fill on every frame at
    // composite time, for little visible gain on flat-color shapes. Re-enable
    // with SetConfigFlags(FLAG_MSAA_4X_HINT) before InitWindow() if smoother
    // edges are preferred over lower GPU usage.
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Aetherium Vanguard - OPTIMIZED");
    SetTargetFPS(60); // Cap the render loop; without it raylib spins at max FPS and pegs a CPU core
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
    game.mapRTBuilt = true;
}
