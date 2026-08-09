#include "game.h"

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
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!game.particles[i].active) continue;
        Particle* p = &game.particles[i];
        float progress = p->life / p->maxLife;
        Color color = ColorLerp(p->startColor, p->endColor, progress);
        float size = Lerp(p->startSize, p->endSize, progress);
        color = ColorTint(color, game.environmentColor);
        DrawCircleV(p->position, size, color);
    }

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
    for (int i = 0; i < count; i++) {
        // Free-list allocation
        int index = -1;
        for (int j = game.nextFreeParticle; j < MAX_PARTICLES; j++) {
            if (!game.particles[j].active) { index = j; break; }
        }
        if (index == -1) {
            for (int j = 0; j < game.nextFreeParticle; j++) {
                if (!game.particles[j].active) { index = j; break; }
            }
        }
        if (index == -1) return;
        game.nextFreeParticle = (index + 1) % MAX_PARTICLES;

        Particle* p = &game.particles[index];
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
        float magnitude = GetRandomValue(10, (int)speed);
        p->velocity = (Vector2){ cosf(angle) * magnitude, sinf(angle) * magnitude };
        if (gravity && p->velocity.y > 0) p->velocity.y *= -0.5f;
    }
}

void AddFloatingText(Vector2 position, const char* text, Color color, bool critical) {
    int index = -1;
    for (int i = game.nextFreeFloatingText; i < MAX_FLOATING_TEXT; i++) {
        if (!game.floatingTexts[i].active) { index = i; break; }
    }
    if (index == -1) {
        for (int i = 0; i < game.nextFreeFloatingText; i++) {
            if (!game.floatingTexts[i].active) { index = i; break; }
        }
    }
    if (index == -1) return;
    game.nextFreeFloatingText = (index + 1) % MAX_FLOATING_TEXT;

    FloatingText* ft = &game.floatingTexts[index];
    ft->active = true;
    ft->position = (Vector2){position.x + GetRandomValue(-5, 5), position.y + GetRandomValue(-5, 5)};
    strncpy(ft->text, text, 31);
    ft->text[31] = '\0';
    ft->color = color;
    ft->lifetime = critical ? 2.0f : 1.5f;
    ft->critical = critical;
    ft->velocityY = critical ? -80.0f : -40.0f;
}

void AddFloatingTextFmt(Vector2 position, Color color, bool critical, const char* fmt, ...) {
    int index = -1;
    for (int i = game.nextFreeFloatingText; i < MAX_FLOATING_TEXT; i++) {
        if (!game.floatingTexts[i].active) { index = i; break; }
    }
    if (index == -1) {
        for (int i = 0; i < game.nextFreeFloatingText; i++) {
            if (!game.floatingTexts[i].active) { index = i; break; }
        }
    }
    if (index == -1) return;
    game.nextFreeFloatingText = (index + 1) % MAX_FLOATING_TEXT;

    FloatingText* ft = &game.floatingTexts[index];
    ft->active = true;
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
