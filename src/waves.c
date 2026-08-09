#include "game.h"

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
