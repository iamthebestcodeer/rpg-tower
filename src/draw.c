#include "game.h"
#include "rlgl.h"

static int pendingHeroSkill = -1;

// Called from UpdateHeroLevelUp to apply a skill queued during Draw
void ConsumePendingHeroSkill(void) {
    if (pendingHeroSkill >= 0 && pendingHeroSkill < NUM_HERO_SKILLS && game.hero.skillPoints > 0) {
        game.hero.skills[pendingHeroSkill]++;
        game.hero.skillPoints--;
        ApplyHeroSkills();
    }
    pendingHeroSkill = -1;
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
        DrawText("Controls: WASD (Move), Q (Dash), E (Burst), Space (Attack), 1-4/Click (Build), N (Next Wave)", 20, SCREEN_HEIGHT - 30, 18, COLOR_TEXT_MUTED);
    } else {
        BeginMode2D(game.camera);
        DrawMap();
        DrawEntities();
        DrawVFX();
        EndMode2D();

        DrawUI(game.state == GS_PLAYING);

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

            bool isLevelUp = (game.state == GS_LEVEL_UP_HERO);
            for (int i = 0; i < NUM_HERO_SKILLS; i++) {
                Rectangle btnBounds = {panel.x + 20, startY + i * (buttonHeight + spacing), panel.width - 40, buttonHeight};
                char skillLabel[128];
                snprintf(skillLabel, sizeof(skillLabel), "%s (Level %d)", skillNames[i], game.hero.skills[i]);
                // Record the click; actual mutation happens in UpdateHeroLevelUp
                bool enabled = isLevelUp;
                if (GuiButton(btnBounds, skillLabel, false, enabled)) {
                    pendingHeroSkill = i;
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

}

void DrawEntities(void) {
    DrawEnemies();
    DrawTowers();
    DrawHero();
    DrawProjectiles();
}

static float EnemyDrawSize(EnemyType type) {
    switch (type) {
        case ENEMY_BASIC: return 12.0f;
        case ENEMY_FAST: return 10.0f;
        case ENEMY_TANK: return 18.0f;
        case ENEMY_ETHEREAL: return 14.0f;
        case ENEMY_HEALER: return 15.0f;
        case ENEMY_SPAWNER: return 20.0f;
        case ENEMY_MINION: return 8.0f;
        case ENEMY_BOSS: return 28.0f;
        default: return 10.0f;
    }
}

void DrawEnemies() {
    // Pass 1: batch every base body circle into one triangle fan (zero per-
    // frame trig; see EmitCircleFan). Healer/spawner draw polygons instead
    // and are handled in pass 2 with the overlays. Note that drawing all
    // bodies before all overlays slightly changes cross-enemy z-order (bars
    // and outlines always sit above bodies) - visually negligible, but
    // intentional for the batching win.
    //
    // rlSetTexture(0) binds the 1x1 default white texture so shapes render
    // with vertex color, same as raylib's own shapes path.
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!game.enemies[i].active) continue;
        Enemy* e = &game.enemies[i];
        if (e->type == ENEMY_HEALER || e->type == ENEMY_SPAWNER) continue;

        float size = EnemyDrawSize(e->type);
        Color drawColor = ColorTint(e->visualTint, game.environmentColor);
        EmitCircleFan(e->position, size, drawColor);
    }
    rlEnd();

    // Pass 2: per-enemy overlays (polygons, outlines, auras, status text,
    // health bars) - low volume, kept as individual raylib calls.
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!game.enemies[i].active) continue;
        Enemy* e = &game.enemies[i];
        float size = EnemyDrawSize(e->type);
        Color drawColor = ColorTint(e->visualTint, game.environmentColor);

        if (e->type == ENEMY_HEALER)
            DrawPoly(e->position, 4, size, 45.0f, drawColor);
        else if (e->type == ENEMY_SPAWNER)
            DrawPoly(e->position, 5, size, 0.0f, drawColor);

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
                float angleDiff = GetAngleDifference(t->rotation, angleToTarget);
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
            // Deterministic sparkle: time-based instead of per-frame RNG so the
            // draw path doesn't churn the RNG, and rendering is reproducible
            // for a given game state (same ~1/16 chance per tower per frame).
            int sparkSeed = (int)(game.globalTime * 60.0f) + i * 7;
            if (sparkSeed % 16 == 0) {
                float angle = (float)((sparkSeed + i * 13) % 360) * DEG2RAD;
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
    // Batch projectile bodies (and energy glows) into one triangle fan, same
    // trig-free EmitCircleFan path as particles/enemies. rlSetTexture(0)
    // binds the white default texture (see DrawEnemies for the rationale).
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
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
        EmitCircleFan(p->position, size, color);
        if (p->damageType == DMG_ENERGY)
            EmitCircleFan(p->position, size + 5.0f, Fade(color, 0.4f));
    }
    rlEnd();
}
