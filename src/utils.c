#include "game.h"

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
