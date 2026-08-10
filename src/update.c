#include "game.h"

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
