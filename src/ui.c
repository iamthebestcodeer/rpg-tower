#include "game.h"

//----------------------------------------------------------------------------------
// UI
//----------------------------------------------------------------------------------

void DrawUI(bool interactive) {
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
        DrawTowerInspector(interactive);
    else
        DrawBuildMenu(interactive);
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

void DrawBuildMenu(bool interactive) {
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
        bool hovered = CheckCollisionPointRec(GetMousePosition(), btnBounds);
        bool clickedAffordable = GuiButton(btnBounds, label, selected, canAfford && interactive);
        if (interactive && hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !canAfford) {
            AddFloatingText((Vector2){GAME_AREA_WIDTH + 50, btnBounds.y}, "Not enough Gold!", COLOR_DANGER, false);
        }
        if (clickedAffordable) {
            game.placingTower = types[i];
            game.selectedTowerIndex = -1;
        }
        SetTooltip(name, desc, btnBounds);
    }
}

void DrawTowerInspector(bool interactive) {
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
        if (GuiButton(btnTarget, modeText, false, interactive)) {
            t->targetingMode = (t->targetingMode + 1) % NUM_TARGETING_MODES;
        }
        SetTooltip("Targeting Mode", "Change how the tower prioritizes enemies.", btnTarget);
        currentY += 50;
    }

    if (t->stats.level >= TOWER_BASE_MAX_LEVEL && t->type >= TOWER_PULSE && t->type <= TOWER_TESLA) {
        DrawTowerUpgradePaths(t, interactive);
    }

    Rectangle btnSell = {GAME_AREA_WIDTH + 10, SCREEN_HEIGHT - 50, UI_WIDTH - 20, 40};
    int sellValue = (int)(t->totalCost * 0.6f);
    char sellLabel[64]; snprintf(sellLabel, sizeof(sellLabel), "Sell Tower (%dG)", sellValue);
    if (GuiButton(btnSell, sellLabel, false, interactive)) {
        SellTower(game.selectedTowerIndex);
    }
    SetTooltip("Sell", "Sell the tower for a partial refund.", btnSell);
}

void DrawTowerUpgradePaths(Tower* t, bool interactive) {
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
        bool enabled = interactive && canAfford;
        if (GuiButton(btnBounds, upLabel, false, enabled)) {
            if (UpgradeTower(t, paths[i])) {
                game.gold -= costGold;
                game.aether -= costAether;
                t->totalCost += costGold;
                AddFloatingText(t->position, "UPGRADED!", COLOR_AETHER_RES, true);
                ScreenShake(3.0f, 0.3f);
            }
        } else if (interactive && CheckCollisionPointRec(GetMousePosition(), btnBounds)
                   && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !canAfford) {
            AddFloatingText((Vector2){GAME_AREA_WIDTH + 50, btnBounds.y}, "Cannot Afford!", COLOR_DANGER, false);
        }
        SetTooltip(name, desc, btnBounds);
    }
}

void DrawTooltip(void) {
    if (!game.tooltip.visible) return;
    Vector2 mousePos = GetMousePosition();
    int padding = 10, titleSize = 20, textSize = 16, maxWidth = 300;

    // Split description by newlines and measure widest segment
    int longestLineWidth = 0;
    int lineCount = 1;
    const char *lineStart = game.tooltip.description;
    for (const char* p = game.tooltip.description; ; p++) {
        if (*p == '\n' || *p == '\0') {
            int len = (int)(p - lineStart);
            char tmp[512];
            int cpy = len < (int)sizeof(tmp)-1 ? len : (int)sizeof(tmp)-1;
            memcpy(tmp, lineStart, cpy); tmp[cpy] = '\0';
            int w = MeasureText(tmp, textSize);
            if (w > longestLineWidth) longestLineWidth = w;
            lineCount += (*p == '\n') ? 1 : 0;
            if (*p == '\0') break;
            lineStart = p + 1;
        }
    }
    int textHeight = lineCount * (textSize + 5);
    int titleHeight = titleSize + 5;
    int width = MeasureText(game.tooltip.title, titleSize);
    if (longestLineWidth > width) width = longestLineWidth;
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
    // Draw each line separately so long/wrapped text stays inside the box
    float ty = tooltipRect.y + padding * 2 + titleHeight;
    lineStart = game.tooltip.description;
    for (const char* p = game.tooltip.description; ; p++) {
        if (*p == '\n' || *p == '\0') {
            int len = (int)(p - lineStart);
            char tmp[512];
            int cpy = len < (int)sizeof(tmp)-1 ? len : (int)sizeof(tmp)-1;
            memcpy(tmp, lineStart, cpy); tmp[cpy] = '\0';
            DrawText(tmp, tooltipRect.x + padding, (int)ty, textSize, COLOR_TEXT_PRIMARY);
            ty += textSize + 5;
            if (*p == '\0') break;
            lineStart = p + 1;
        }
    }
}

bool GuiButton(Rectangle bounds, const char* text, bool selected, bool enabled) {
    Vector2 mousePos = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mousePos, bounds);
    bool clicked = hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    Color bgColor = COLOR_UI_ACCENT;
    Color textColor = COLOR_TEXT_PRIMARY;
    Color borderColor = COLOR_GRID;

    if (!enabled) {
        bgColor = ColorBrightness(COLOR_UI_ACCENT, -0.4f);
        textColor = COLOR_TEXT_MUTED;
        // Still detect hover for border feedback even when disabled
        if (hovered) borderColor = YELLOW;
    } else if (selected) {
        bgColor = COLOR_ENERGY;
        textColor = BLACK;
        borderColor = YELLOW;
    } else if (hovered) {
        bgColor = ColorBrightness(COLOR_UI_ACCENT, 0.2f);
        borderColor = YELLOW;
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
