#include "game.h"
#include "rlgl.h"

//----------------------------------------------------------------------------------
// VFX
//----------------------------------------------------------------------------------

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

void DrawVFX(void) {
    // Batch every active particle into a single triangle fan: raylib's
    // DrawCircleV() emits a 36-segment circle and recomputes sinf()/cosf() per
    // segment per circle (plus a per-circle rlBegin/rlEnd), so MAX_PARTICLES
    // alive can cost hundreds of thousands of trig calls per frame. The
    // precomputed unit circle in EmitCircleFan makes this pure vertex
    // submission with no per-frame trig.
    //
    // NOTE: bind the 1x1 default white texture (rlSetTexture(0)) so shapes
    // render with vertex color, mirroring what raylib's own shapes drawing
    // relies on. Do NOT use GetShapesTexture() here - it is the font atlas,
    // and vertices would sample it with stale texcoords.
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!game.particles[i].active) continue;
        Particle* p = &game.particles[i];
        float progress = p->life / p->maxLife;
        Color color = ColorLerp(p->startColor, p->endColor, progress);
        float size = Lerp(p->startSize, p->endSize, progress);
        color = ColorTint(color, game.environmentColor);
        EmitCircleFan(p->position, size, color);
    }
    rlEnd();

    for (int i = 0; i < MAX_FLOATING_TEXT; i++) {
        if (!game.floatingTexts[i].active) continue;
        FloatingText* ft = &game.floatingTexts[i];
        float alpha = Clamp(ft->lifetime / 0.5f, 0.0f, 1.0f);
        int fontSize = ft->critical ? 24 : 20;
        // Use DrawText for no rotation
        DrawText(ft->text, (int)ft->position.x + 1, (int)ft->position.y + 1, fontSize, Fade(BLACK, alpha));
        DrawText(ft->text, (int)ft->position.x, (int)ft->position.y, fontSize, Fade(ft->color, alpha));
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
