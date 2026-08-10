#include "game.h"

//----------------------------------------------------------------------------------
// VFX
//----------------------------------------------------------------------------------

// Per-frame budget for new floating texts. Heavy fire (multiple fast towers)
// can request dozens of damage numbers per frame, each costing a formatted
// string + pool scan; capping creation keeps churn bounded and reduces on-screen
// clutter. Generous enough that normal play never notices.
#define MAX_FLOATING_TEXT_PER_FRAME 48
static int g_floatingTextBudget = MAX_FLOATING_TEXT_PER_FRAME;

void ResetFloatingTextBudget(void) {
    g_floatingTextBudget = MAX_FLOATING_TEXT_PER_FRAME;
}

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

//----------------------------------------------------------------------------------
// Particle sprite
//----------------------------------------------------------------------------------

// Soft particle sprite: solid core with radial falloff to transparent. Each
// particle is drawn as one tinted DrawTexturePro quad, which (a) renders
// independently of the rlgl batch overflow the old manual triangle-fan pass
// relied on, and (b) gives a soft anti-aliased edge. The falloff edge maps to
// the particle's radius, so the perceived size matches the old solid discs;
// the core fraction keeps the center fully opaque.
#define PARTICLE_SPRITE_SIZE 64
#define PARTICLE_SPRITE_CORE 0.70f // solid core, as a fraction of half-size
#define PARTICLE_SPRITE_FADE 1.00f // falloff edge, as a fraction of half-size

static Texture2D s_particleSprite = { 0 };

static Texture2D CreateParticleSprite(void) {
    const float half = PARTICLE_SPRITE_SIZE / 2.0f;
    const float coreRadius = PARTICLE_SPRITE_CORE * half;
    const float fadeRadius = PARTICLE_SPRITE_FADE * half;
    Image img = GenImageColor(PARTICLE_SPRITE_SIZE, PARTICLE_SPRITE_SIZE, BLANK);

    for (int y = 0; y < PARTICLE_SPRITE_SIZE; y++) {
        for (int x = 0; x < PARTICLE_SPRITE_SIZE; x++) {
            float dx = x - half + 0.5f;
            float dy = y - half + 0.5f;
            float dist = sqrtf(dx * dx + dy * dy);

            unsigned char alpha = 0;
            if (dist <= coreRadius) {
                alpha = 255;
            } else if (dist < fadeRadius) {
                float t = (dist - coreRadius) / (fadeRadius - coreRadius);
                alpha = (unsigned char)(255.0f * (1.0f - t));
            }
            ImageDrawPixel(&img, x, y, (Color){ 255, 255, 255, alpha });
        }
    }

    Texture2D sprite = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(sprite, TEXTURE_FILTER_BILINEAR); // smooth scaling at any size
    return sprite;
}

void InitVFX(void) {
    if (s_particleSprite.id == 0)
        s_particleSprite = CreateParticleSprite();
}

void UnloadVFX(void) {
    if (s_particleSprite.id != 0) {
        UnloadTexture(s_particleSprite);
        s_particleSprite.id = 0;
    }
}

void DrawVFX(void) {
    // Particles: one tinted DrawTexturePro quad per particle. Every particle
    // shares the same sprite texture, so the pass is still a single rlgl batch
    // - and unlike the old manual triangle fan, it renders regardless of batch
    // size. Sub-pixel particles are skipped to keep the pass cheap.
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!game.particles[i].active) continue;
        Particle* p = &game.particles[i];
        float progress = p->life / p->maxLife;
        Color color = ColorTint(ColorLerp(p->startColor, p->endColor, progress), game.environmentColor);
        float size = Lerp(p->startSize, p->endSize, progress);
        if (size < 0.35f) continue;

        Rectangle dst = { p->position.x - size, p->position.y - size, size * 2.0f, size * 2.0f };
        DrawTexturePro(s_particleSprite, (Rectangle){ 0, 0, (float)s_particleSprite.width, (float)s_particleSprite.height }, dst, (Vector2){ 0, 0 }, 0.0f, color);
    }

    // All floating text (shadows + main) is drawn via DrawTextBatched, which
    // renders glyphs through DrawTexturePro: the font texture never changes
    // between glyphs, so the entire pass stays in one rlgl batch. Shadows are
    // only drawn for critical (large) text to halve glyph vertices for the
    // common small damage numbers.
    {
        Font font = GetFontDefault();
        for (int i = 0; i < MAX_FLOATING_TEXT; i++) {
            if (!game.floatingTexts[i].active) continue;
            FloatingText* ft = &game.floatingTexts[i];
            float alpha = Clamp(ft->lifetime / 0.5f, 0.0f, 1.0f);
            int fontSize = ft->critical ? 24 : 20;
            float x = (float)(int)ft->position.x;
            float y = (float)(int)ft->position.y;
            // Spacing matches raylib's DrawText default (fontSize/10).
            if (ft->critical)
                DrawTextBatched(font, ft->text, (Vector2){ x + 1.0f, y + 1.0f }, (float)fontSize, (float)fontSize/10.0f, Fade(BLACK, alpha));
            DrawTextBatched(font, ft->text, (Vector2){ x, y }, (float)fontSize, (float)fontSize/10.0f, Fade(ft->color, alpha));
        }
    }
}

void SpawnParticles(Vector2 position, int count, Color startColor, Color endColor, float speed, float sizeStart, float sizeEnd, bool gravity) {
    // Free-list allocation, but scan the pool ONCE for up to `count` free
    // slots instead of re-scanning from the head for every individual
    // particle. The old loop was O(count * MAX_PARTICLES) worst case when the
    // pool was densely populated (e.g. a 200-particle burst against ~3000
    // live particles); a single pass is O(MAX_PARTICLES) total.
    int spawned = 0;
    int lastIndex = -1;

    // Pass 0: head -> end; pass 1: wrap-around 0 -> head (same slots the old
    // two-phase scan visited, just collected in one sweep).
    for (int pass = 0; pass < 2 && spawned < count; pass++) {
        int start = (pass == 0) ? game.nextFreeParticle : 0;
        int end   = (pass == 0) ? MAX_PARTICLES : game.nextFreeParticle;
        for (int j = start; j < end && spawned < count; j++) {
            if (game.particles[j].active) continue;

            Particle* p = &game.particles[j];
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
            // Clamp the random magnitude so slow/small effects still emit
            // particles (matches the remote clamp from the rebased branch).
            int maxMagnitude = (int)speed;
            int minMagnitude = (maxMagnitude < 10) ? 0 : 10;
            if (minMagnitude > maxMagnitude) minMagnitude = maxMagnitude;
            float magnitude = (float)GetRandomValue(minMagnitude, maxMagnitude);
            p->velocity = (Vector2){ cosf(angle) * magnitude, sinf(angle) * magnitude };
            if (gravity && p->velocity.y > 0) p->velocity.y *= -0.5f;

            lastIndex = j;
            spawned++;
        }
    }

    if (lastIndex >= 0)
        game.nextFreeParticle = (lastIndex + 1) % MAX_PARTICLES;
}

static FloatingText *ReserveFloatingTextSlot(void) {
    if (g_floatingTextBudget <= 0) return NULL;
    g_floatingTextBudget--;

    int index = -1;
    for (int i = game.nextFreeFloatingText; i < MAX_FLOATING_TEXT; i++) {
        if (!game.floatingTexts[i].active) { index = i; break; }
    }
    if (index == -1) {
        for (int i = 0; i < game.nextFreeFloatingText; i++) {
            if (!game.floatingTexts[i].active) { index = i; break; }
        }
    }
    if (index == -1) return NULL;
    game.nextFreeFloatingText = (index + 1) % MAX_FLOATING_TEXT;
    FloatingText *ft = &game.floatingTexts[index];
    ft->active = true;
    ft->position = (Vector2){0, 0};
    ft->text[0] = '\0';
    ft->lifetime = 1.5f;
    ft->critical = false;
    ft->velocityY = -40.0f;
    return ft;
}

void AddFloatingText(Vector2 position, const char* text, Color color, bool critical) {
    FloatingText *ft = ReserveFloatingTextSlot();
    if (!ft) return;
    ft->position = (Vector2){position.x + GetRandomValue(-5, 5), position.y + GetRandomValue(-5, 5)};
    strncpy(ft->text, text, 31);
    ft->text[31] = '\0';
    ft->color = color;
    ft->lifetime = critical ? 2.0f : 1.5f;
    ft->critical = critical;
    ft->velocityY = critical ? -80.0f : -40.0f;
}

void AddFloatingTextFmt(Vector2 position, Color color, bool critical, const char* fmt, ...) {
    FloatingText *ft = ReserveFloatingTextSlot();
    if (!ft) return;
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
