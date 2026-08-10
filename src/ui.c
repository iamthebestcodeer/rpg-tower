#include "game.h"

//----------------------------------------------------------------------------------
// Sidebar Layout
//----------------------------------------------------------------------------------
// The sidebar is a fixed column to the right of the play field. Every menu and
// the tower inspector live there, so all controls are positioned from these
// constants instead of raw pixel values.

#define SIDEBAR_X      GAME_AREA_WIDTH
#define SIDEBAR_MARGIN 10
#define BUTTON_WIDTH   (UI_WIDTH - 20)

#define BUTTON_GAP   10
#define SECTION_GAP  10

// Tower inspector
#define SECTION_HEADER_Y   270
#define INSPECTOR_TITLE_Y  300
#define INSPECTOR_KILLS_Y  328
#define INSPECTOR_BODY_Y   348
#define SELL_BUTTON_Y      (SCREEN_HEIGHT - 50)
#define SELL_BUTTON_HEIGHT 40
#define UPGRADE_HEADER_SPACING  30
#define MIN_UPGRADE_BUTTON_HEIGHT 40
#define NUM_UPGRADE_PATHS 2

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
    DrawText("BUILD MENU (1-4)", SIDEBAR_X + SIDEBAR_MARGIN, SECTION_HEADER_Y, 20, COLOR_ENERGY);
    int startY = 300, buttonHeight = 65, spacing = 15;
    TowerType types[] = {TOWER_PULSE, TOWER_CANNON, TOWER_CRYO, TOWER_TESLA};

    for (int i = 0; i < 4; i++) {
        Rectangle btnBounds = {SIDEBAR_X + SIDEBAR_MARGIN, startY + i * (buttonHeight + spacing), BUTTON_WIDTH, buttonHeight};
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

//----------------------------------------------------------------------------------
// Tower Inspector
//----------------------------------------------------------------------------------

static bool HasSelectedTower(void) {
    if (game.selectedTowerIndex == -1 || !game.towers[game.selectedTowerIndex].active) {
        game.selectedTowerIndex = -1;
        return false;
    }
    return true;
}

static bool IsBaseTower(const Tower* tower) {
    return tower->type >= TOWER_PULSE && tower->type <= TOWER_TESLA;
}

static bool IsEligibleForUpgrade(const Tower* tower) {
    return tower->stats.level >= TOWER_BASE_MAX_LEVEL && IsBaseTower(tower);
}

static const char* DamageTypeName(DamageType type) {
    switch (type) {
        case DMG_ENERGY:   return "Energy";
        case DMG_PHYSICAL: return "Physical";
        default:           return "True";
    }
}

static const char* RangeText(float range) {
    static char buffer[32];
    if (range > SCREEN_WIDTH * 2) return "Global";
    snprintf(buffer, sizeof(buffer), "%.0f", range);
    return buffer;
}

static const char* TargetingModeText(TargetingMode mode) {
    switch (mode) {
        case TARGET_FIRST:     return "First (Default)";
        case TARGET_CLOSEST:   return "Closest";
        case TARGET_STRONGEST: return "Strongest (HP)";
        case TARGET_WEAKEST:   return "Weakest (HP)";
        default:               return "Unknown";
    }
}

static void DrawTowerInspectorHeader(const Tower* tower) {
    DrawText("TOWER INSPECTION", SIDEBAR_X + SIDEBAR_MARGIN, SECTION_HEADER_Y, 20, YELLOW);

    char title[128];
    snprintf(title, sizeof(title), "%s (L%d)", GetTowerName(tower->type), tower->stats.level);
    DrawText(title, SIDEBAR_X + SIDEBAR_MARGIN, INSPECTOR_TITLE_Y, 22, GetTowerColor(tower->type));

    // Kills goes on its own line below the title: the tower name at 22px is
    // nearly as wide as the sidebar, so sharing the title's line would
    // overlap the two texts.
    char kills[32];
    snprintf(kills, sizeof(kills), "Kills: %d", tower->kills);
    DrawText(kills, SIDEBAR_X + SIDEBAR_MARGIN, INSPECTOR_KILLS_Y, 16, COLOR_TEXT_PRIMARY);
}

static void DrawTowerXpBar(const Tower* tower, int y) {
    Rectangle xpBar = {SIDEBAR_X + SIDEBAR_MARGIN, y, BUTTON_WIDTH, 20};
    if (tower->stats.level < TOWER_BASE_MAX_LEVEL) {
        float xpPercent = (tower->stats.xpToNextLevel > 0)
            ? (float)tower->stats.xp / tower->stats.xpToNextLevel : 0.0f;
        DrawRectangleRec(xpBar, COLOR_UI_ACCENT);
        DrawRectangle(xpBar.x, xpBar.y, (int)(xpBar.width * xpPercent), xpBar.height, COLOR_XP);
        DrawRectangleLinesEx(xpBar, 1, BLACK);
        char label[64];
        snprintf(label, sizeof(label), "%d / %d", tower->stats.xp, tower->stats.xpToNextLevel);
        DrawText(label, xpBar.x + 10, xpBar.y + 2, 16, COLOR_TEXT_PRIMARY);
    } else {
        DrawRectangleRec(xpBar, COLOR_XP);
        DrawText("MAX LEVEL - READY FOR UPGRADE", xpBar.x + 10, xpBar.y + 2, 16, BLACK);
    }
}

static int DrawTowerExperienceSection(const Tower* tower, int y) {
    if (IsBaseTower(tower)) {
        DrawText("Experience:", SIDEBAR_X + SIDEBAR_MARGIN, y, 18, COLOR_XP);
        DrawTowerXpBar(tower, y + 20);
        return y + 50;
    }
    DrawText("Specialized Tower (Max Tier)", SIDEBAR_X + SIDEBAR_MARGIN, y, 18, COLOR_XP);
    return y + 30;
}

static void DrawFireRateLine(const Tower* tower, int y) {
    int x = SIDEBAR_X + SIDEBAR_MARGIN;
    if (tower->type == TOWER_CRYO || tower->type == TOWER_T4_CRYO_FREEZER) {
        DrawText("Fire Rate: Continuous (DPS)", x, y, 18, COLOR_TEXT_PRIMARY);
    } else if (tower->type == TOWER_T4_CRYO_BLIZZARD) {
        char line[64];
        snprintf(line, sizeof(line), "Slow Aura: %.0f%%", tower->stats.fireRate * 100.0f);
        DrawText(line, x, y, 18, COLOR_TEXT_PRIMARY);
    } else {
        char line[64];
        snprintf(line, sizeof(line), "Fire Rate: %.2f/s", tower->stats.fireRate);
        DrawText(line, x, y, 18, COLOR_TEXT_PRIMARY);
    }
}

static int DrawTowerStatsSection(const Tower* tower, int y) {
    y += 10;
    DrawText("Stats:", SIDEBAR_X + SIDEBAR_MARGIN, y, 20, YELLOW);
    y += 25;

    char damage[64];
    snprintf(damage, sizeof(damage), "Damage: %.1f (%s)", tower->stats.damage, DamageTypeName(tower->damageType));
    DrawText(damage, SIDEBAR_X + SIDEBAR_MARGIN, y, 18, GetTowerColor(tower->type));
    y += 25;

    char range[64];
    snprintf(range, sizeof(range), "Range: %s", RangeText(tower->stats.range));
    DrawText(range, SIDEBAR_X + SIDEBAR_MARGIN, y, 18, COLOR_TEXT_PRIMARY);
    y += 25;

    DrawFireRateLine(tower, y);
    y += 35;

    DrawLine(SIDEBAR_X, y, SCREEN_WIDTH, y, COLOR_UI_ACCENT);
    return y + 10;
}

static int DrawTargetingModeSection(Tower* tower, int y, bool interactive) {
    DrawText("Targeting Mode:", SIDEBAR_X + SIDEBAR_MARGIN, y, 20, YELLOW);
    y += 25;

    Rectangle button = {SIDEBAR_X + SIDEBAR_MARGIN, y, BUTTON_WIDTH, 40};
    if (GuiButton(button, TargetingModeText(tower->targetingMode), false, interactive)) {
        tower->targetingMode = (tower->targetingMode + 1) % NUM_TARGETING_MODES;
    }
    SetTooltip("Targeting Mode", "Change how the tower prioritizes enemies.", button);

    return y + 50;
}

static void DrawSellButton(const Tower* tower, bool interactive) {
    Rectangle button = {SIDEBAR_X + SIDEBAR_MARGIN, SELL_BUTTON_Y, BUTTON_WIDTH, SELL_BUTTON_HEIGHT};
    int sellValue = (int)(tower->totalCost * 0.6f);
    char label[64];
    snprintf(label, sizeof(label), "Sell Tower (%dG)", sellValue);
    if (GuiButton(button, label, false, interactive)) {
        SellTower(game.selectedTowerIndex);
    }
    SetTooltip("Sell", "Sell the tower for a partial refund.", button);
}

void DrawTowerInspector(bool interactive) {
    if (!HasSelectedTower()) return;

    Tower* tower = &game.towers[game.selectedTowerIndex];

    DrawTowerInspectorHeader(tower);

    int y = INSPECTOR_BODY_Y;
    y = DrawTowerExperienceSection(tower, y);
    y = DrawTowerStatsSection(tower, y);
    if (tower->type != TOWER_T4_CRYO_BLIZZARD) {
        y = DrawTargetingModeSection(tower, y, interactive);
    }
    if (IsEligibleForUpgrade(tower)) {
        DrawTowerUpgradePaths(tower, y, interactive);
    }
    DrawSellButton(tower, interactive);
}

//----------------------------------------------------------------------------------
// Tower Upgrade Paths
//----------------------------------------------------------------------------------

static bool GetTowerUpgradePaths(TowerType type, TowerType paths[NUM_UPGRADE_PATHS]) {
    switch (type) {
        case TOWER_PULSE:
            paths[0] = TOWER_T4_PULSE_REPEATER;
            paths[1] = TOWER_T4_PULSE_SNIPER;
            return true;
        case TOWER_CANNON:
            paths[0] = TOWER_T4_CANNON_MORTAR;
            paths[1] = TOWER_T4_CANNON_VULCAN;
            return true;
        case TOWER_CRYO:
            paths[0] = TOWER_T4_CRYO_BLIZZARD;
            paths[1] = TOWER_T4_CRYO_FREEZER;
            return true;
        case TOWER_TESLA:
            paths[0] = TOWER_T4_TESLA_CHAIN;
            paths[1] = TOWER_T4_TESLA_STORM;
            return true;
        default:
            return false;
    }
}

static void ApplyTowerUpgrade(Tower* tower, TowerType newType, int goldCost, int aetherCost) {
    if (!UpgradeTower(tower, newType)) return;
    game.gold -= goldCost;
    game.aether -= aetherCost;
    tower->totalCost += goldCost;
    AddFloatingText(tower->position, "UPGRADED!", COLOR_AETHER_RES, true);
    ScreenShake(3.0f, 0.3f);
}

// Tier-4 tower names carry a redundant "T4: " prefix. The upgrade menu header
// already says T4, and keeping the prefix makes the button label wider than
// the sidebar button (its right end would clip at the screen edge).
static const char* ShortTowerName(TowerType type) {
    const char* name = GetTowerName(type);
    return (strncmp(name, "T4: ", 4) == 0) ? name + 4 : name;
}

static void DrawUpgradeButton(Tower* tower, TowerType path, Rectangle bounds, bool interactive) {
    int goldCost = GetTowerCost(path);
    int aetherCost = GetTowerAetherCost(path);
    bool canAfford = (game.gold >= goldCost) && (game.aether >= aetherCost);
    bool enabled = interactive && canAfford;

    const char* name = ShortTowerName(path);
    const char* description = GetTowerDescription(path);
    char label[128];
    snprintf(label, sizeof(label), "%s (%dG, %dA)", name, goldCost, aetherCost);

    if (GuiButton(bounds, label, false, enabled)) {
        ApplyTowerUpgrade(tower, path, goldCost, aetherCost);
    } else if (interactive && CheckCollisionPointRec(GetMousePosition(), bounds)
               && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !canAfford) {
        AddFloatingText((Vector2){SIDEBAR_X + 50, bounds.y}, "Cannot Afford!", COLOR_DANGER, false);
    }
    SetTooltip(name, description, bounds);
}

void DrawTowerUpgradePaths(Tower* tower, int startY, bool interactive) {
    TowerType paths[NUM_UPGRADE_PATHS];
    if (!GetTowerUpgradePaths(tower->type, paths)) return;

    int y = startY;
    // The aether cost is already shown in each button label, so the header
    // stays short enough to fit the sidebar width.
    DrawText("T4 UPGRADE PATHS", SIDEBAR_X + SIDEBAR_MARGIN, y, 20, COLOR_AETHER_RES);
    y += UPGRADE_HEADER_SPACING;

    // The sell button is pinned to the bottom of the sidebar, so the upgrade
    // buttons must fit into the space between this section and that button.
    int availableHeight = SELL_BUTTON_Y - SECTION_GAP - y;
    int buttonHeight = (availableHeight - BUTTON_GAP) / NUM_UPGRADE_PATHS;
    if (buttonHeight < MIN_UPGRADE_BUTTON_HEIGHT) {
        buttonHeight = MIN_UPGRADE_BUTTON_HEIGHT;
    }

    for (int i = 0; i < NUM_UPGRADE_PATHS; i++) {
        Rectangle bounds = {SIDEBAR_X + SIDEBAR_MARGIN, y, BUTTON_WIDTH, buttonHeight};
        DrawUpgradeButton(tower, paths[i], bounds, interactive);
        y += buttonHeight + BUTTON_GAP;
    }
}

#define TOOLTIP_MAX_LINES 16
#define TOOLTIP_LINE_CAP  128

// Wrap a tooltip description into lines that each fit within maxWidth, breaking
// at word boundaries (source newlines force a break). Long single-line tower
// descriptions are ~900px at 16px - far wider than the 300px tooltip box - so
// without wrapping the text would run off the right edge of the screen.
// Returns the number of lines written to `lines`.
static int WrapTooltipDescription(const char* description, int maxWidth, int textSize,
                                  char lines[TOOLTIP_MAX_LINES][TOOLTIP_LINE_CAP])
{
    int lineCount = 0;
    char line[TOOLTIP_LINE_CAP];
    int lineLen = 0;
    bool lineHasWord = false;

    const char* p = description;
    while (*p != '\0' && lineCount < TOOLTIP_MAX_LINES) {
        // Copy the next word (bounded by space/newline/end of string).
        char word[TOOLTIP_LINE_CAP];
        int wordLen = 0;
        while (*p != '\0' && *p != ' ' && *p != '\n' && wordLen < TOOLTIP_LINE_CAP - 1) {
            word[wordLen++] = *p++;
        }
        word[wordLen] = '\0';

        if (wordLen > 0) {
            // Would adding this word overflow the current line?
            char trial[TOOLTIP_LINE_CAP];
            memcpy(trial, line, lineLen);
            int pos = lineLen;
            // Only append the separating space if the line can still hold a
            // terminator; otherwise pos would reach TOOLTIP_LINE_CAP and the
            // trial[pos + copyLen] = '\0' write below would run 1 byte past
            // the buffer (a 127-char line + another word triggers it).
            if (lineHasWord && pos < TOOLTIP_LINE_CAP - 1) trial[pos++] = ' ';
            int copyLen = wordLen;
            if (pos + copyLen > TOOLTIP_LINE_CAP - 1) copyLen = TOOLTIP_LINE_CAP - 1 - pos;
            if (copyLen < 0) copyLen = 0;
            memcpy(trial + pos, word, copyLen);
            trial[pos + copyLen] = '\0';

            if (lineHasWord && MeasureText(trial, textSize) > maxWidth) {
                // The word doesn't fit on the current line: flush that line
                // and start the next one with this word.
                if (lineCount < TOOLTIP_MAX_LINES) {
                    line[lineLen] = '\0';
                    memcpy(lines[lineCount++], line, lineLen + 1);
                }
                lineLen = wordLen;
                memcpy(line, word, wordLen + 1);
                lineHasWord = true;
            } else {
                lineLen = pos + copyLen;
                memcpy(line, trial, lineLen + 1);
                lineHasWord = true;
            }
        }

        if (*p == ' ') {
            p++;
        } else if (*p == '\n') {
            // Source newline: flush the current line and start a new one.
            if (lineCount < TOOLTIP_MAX_LINES) {
                line[lineLen] = '\0';
                memcpy(lines[lineCount++], line, lineLen + 1);
            }
            lineLen = 0;
            lineHasWord = false;
            p++;
        }
    }

    // Flush the final line (an empty description still yields one line).
    if (lineCount < TOOLTIP_MAX_LINES) {
        line[lineLen] = '\0';
        memcpy(lines[lineCount++], line, lineLen + 1);
    }
    return lineCount;
}

void DrawTooltip(void) {
    if (!game.tooltip.visible) return;
    Vector2 mousePos = GetMousePosition();
    int padding = 10, titleSize = 20, textSize = 16, maxWidth = 300;

    char lines[TOOLTIP_MAX_LINES][TOOLTIP_LINE_CAP];
    int lineCount = WrapTooltipDescription(game.tooltip.description, maxWidth, textSize, lines);

    int width = MeasureText(game.tooltip.title, titleSize);
    for (int i = 0; i < lineCount; i++) {
        int lineWidth = MeasureText(lines[i], textSize);
        if (lineWidth > width) width = lineWidth;
    }
    if (width > maxWidth) width = maxWidth;

    int textHeight = lineCount * (textSize + 5);
    int titleHeight = titleSize + 5;

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

    float y = tooltipRect.y + padding * 2 + titleHeight;
    for (int i = 0; i < lineCount; i++) {
        DrawText(lines[i], tooltipRect.x + padding, (int)y, textSize, COLOR_TEXT_PRIMARY);
        y += textSize + 5;
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
