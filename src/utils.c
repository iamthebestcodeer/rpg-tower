#include "game.h"
#include "rlgl.h"

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
    float difference = fmodf(end - start, 360.0f);
    if (difference < 0.0f) difference += 360.0f;
    if (difference > 180.0f) difference -= 360.0f;
    return start + difference * amount;
}

float GetAngleDifference(float a, float b) {
    float d = fmodf(a - b, 360.0f);
    if (d < 0.0f) d += 360.0f;
    if (d > 180.0f) d -= 360.0f;
    return fabsf(d);
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

Color GetTowerColor(TowerType type) {
    switch (type) {
        case TOWER_PULSE: case TOWER_T4_PULSE_REPEATER: case TOWER_T4_PULSE_SNIPER: return COLOR_ENERGY;
        case TOWER_CANNON: case TOWER_T4_CANNON_MORTAR: case TOWER_T4_CANNON_VULCAN: return COLOR_PHYSICAL;
        case TOWER_CRYO: case TOWER_T4_CRYO_BLIZZARD: case TOWER_T4_CRYO_FREEZER: return COLOR_CRYO;
        case TOWER_TESLA: case TOWER_T4_TESLA_CHAIN: case TOWER_T4_TESLA_STORM: return COLOR_TESLA;
        default: return GRAY;
    }
}

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

//----------------------------------------------------------------------------------
// Batched Rendering
//----------------------------------------------------------------------------------

// Adaptive circle tessellation. raylib's DrawCircleV() always emits a
// 36-segment fan (108 vertices) and recomputes sinf()/cosf() per segment, so a
// full particle field costs ~400k vertices/frame. Tiny circles don't need 36
// segments, so we pick a segment count by radius (6..36) and reuse precomputed
// unit circles, turning each circle into pure vertex submission with ~3-6x
// fewer vertices for the typical 2-8px particle. Called inside a single
// rlBegin(RL_TRIANGLES) block shared by all emitters.
#define CIRCLE_LEVELS 7
static const int circleSegments[CIRCLE_LEVELS] = { 4, 6, 8, 12, 18, 24, 36 };
static Vector2 unitCircles[CIRCLE_LEVELS][37];
static bool unitCirclesBuilt = false;

static int CircleLevelForRadius(float radius)
{
    if (radius < 2.2f) return 0;   //  4 segments (diamond - most particles)
    if (radius < 5.0f) return 1;   //  6 segments
    if (radius < 9.0f) return 2;   //  8 segments
    if (radius < 15.0f) return 3;  // 12 segments
    if (radius < 28.0f) return 4;  // 18 segments
    if (radius < 40.0f) return 5;  // 24 segments (large auras/circles)
    return 6;                      // 36 segments (very large - matches DrawCircleV)
}

void EmitCircleFan(Vector2 center, float radius, Color color)
{
    if (radius < 0.35f) return; // sub-pixel: invisible, skip entirely

    if (!unitCirclesBuilt) {
        for (int l = 0; l < CIRCLE_LEVELS; l++) {
            int n = circleSegments[l];
            float step = 360.0f / (float)n;
            for (int i = 0; i <= n; i++) {
                float angle = i * step * DEG2RAD;
                unitCircles[l][i] = (Vector2){ cosf(angle), sinf(angle) };
            }
        }
        unitCirclesBuilt = true;
    }

    int level = CircleLevelForRadius(radius);
    int n = circleSegments[level];
    const Vector2* uc = unitCircles[level];

    rlColor4ub(color.r, color.g, color.b, color.a);
    for (int i = 0; i < n; i++) {
        rlVertex2f(center.x, center.y);
        rlVertex2f(center.x + uc[i + 1].x * radius, center.y + uc[i + 1].y * radius);
        rlVertex2f(center.x + uc[i].x * radius, center.y + uc[i].y * radius);
    }
}

// Filled rect into the current rlBegin(RL_TRIANGLES) batch (2 triangles).
void EmitRect(Rectangle rec, Color color)
{
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlVertex2f(rec.x, rec.y);
    rlVertex2f(rec.x + rec.width, rec.y);
    rlVertex2f(rec.x + rec.width, rec.y + rec.height);
    rlVertex2f(rec.x + rec.width, rec.y + rec.height);
    rlVertex2f(rec.x, rec.y + rec.height);
    rlVertex2f(rec.x, rec.y);
}

// Matches raylib's default textLineSpacing (settable via SetTextLineSpacing()).
#define TEXT_LINE_SPACING 10.0f

// Draw one glyph at the given position, replicating raylib 6.0's
// DrawTextCodepoint() exactly (glyph padding and scaleFactor included).
static void DrawGlyph(Font font, int glyphIndex, Vector2 position, float scaleFactor, Color tint)
{
    Rectangle srcRec = {
        font.recs[glyphIndex].x - (float)font.glyphPadding,
        font.recs[glyphIndex].y - (float)font.glyphPadding,
        font.recs[glyphIndex].width + 2.0f * (float)font.glyphPadding,
        font.recs[glyphIndex].height + 2.0f * (float)font.glyphPadding
    };
    Rectangle dstRec = {
        position.x + font.glyphs[glyphIndex].offsetX * scaleFactor - (float)font.glyphPadding * scaleFactor,
        position.y + font.glyphs[glyphIndex].offsetY * scaleFactor - (float)font.glyphPadding * scaleFactor,
        (font.recs[glyphIndex].width + 2.0f * (float)font.glyphPadding) * scaleFactor,
        (font.recs[glyphIndex].height + 2.0f * (float)font.glyphPadding) * scaleFactor
    };
    DrawTexturePro(font.texture, srcRec, dstRec, (Vector2){ 0, 0 }, 0.0f, tint);
}

// Draw a text string with DrawTextEx's exact glyph placement, using one
// DrawTexturePro per glyph.
//
// WHY NOT rlgl immediate mode: manual rlBegin(RL_QUADS)/rlVertex* quads are
// invisible on this raylib 6.0 build (only geometry that overflows the
// 8192-vertex batch renders; small text batches vanish at the end-of-frame
// flush). DrawTexturePro goes through raylib's own batched path and always
// renders. It is still ONE rlgl batch for a whole text pass: consecutive
// same-texture DrawTexturePro calls do not split the draw entry, so the
// batching win from the old manual quads holds.
void DrawTextBatched(Font font, const char* text, Vector2 position, float fontSize, float spacing, Color tint)
{
    if (font.texture.id == 0) font = GetFontDefault(); // Same guard as DrawTextEx

    float scaleFactor = fontSize / (float)font.baseSize;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    int length = (int)TextLength(text);

    for (int i = 0; i < length; )
    {
        int codepointSize = 0;
        int codepoint = GetCodepointNext(text + i, &codepointSize);
        int glyphIndex = GetGlyphIndex(font, codepoint);

        if (codepoint == '\n')
        {
            offsetY += fontSize + TEXT_LINE_SPACING; // matches DrawTextEx
            offsetX = 0.0f;
        }
        else
        {
            if ((codepoint != ' ') && (codepoint != '\t'))
                DrawGlyph(font, glyphIndex, (Vector2){ position.x + offsetX, position.y + offsetY }, scaleFactor, tint);

            // Glyph advance, exactly as DrawTextEx computes it: spacing applies
            // to the unscaled advance, and advanceX == 0 falls back to the
            // glyph width for fonts without advance metrics.
            float advance = (font.glyphs[glyphIndex].advanceX == 0)
                ? (float)font.recs[glyphIndex].width
                : (float)font.glyphs[glyphIndex].advanceX;
            offsetX += advance * scaleFactor + spacing;
        }
        i += codepointSize;
    }
}
