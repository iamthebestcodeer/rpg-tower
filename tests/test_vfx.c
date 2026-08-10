// Tests for vfx.c: particles, floating text, screen shake, VFX update/draw.
#include "test_util.h"

static void TestUpdateVFX(void) {
    ResetForTest();
    AddFloatingText((Vector2){100, 100}, "HI", WHITE, false);
    CHECK(game.floatingTexts[0].active);
    CHECK(game.floatingTexts[0].lifetime == 1.5f);
    float y0 = game.floatingTexts[0].position.y;
    UpdateVFX(1.0f);
    CHECK(game.floatingTexts[0].position.y < y0); // drifts up
    UpdateVFX(1.0f);
    CHECK(!game.floatingTexts[0].active); // expired

    // particles: gravity + expiry
    ResetForTest();
    game.particles[0].active = true;
    game.particles[0].life = 0;
    game.particles[0].maxLife = 0.5f;
    game.particles[0].gravity = true;
    game.particles[0].velocity = (Vector2){10, 10};
    game.particles[0].position = (Vector2){0, 0};
    UpdateVFX(0.1f);
    CHECK(game.particles[0].velocity.y > 10.0f); // gravity applied
    CHECK(game.particles[0].active);
    UpdateVFX(0.5f);
    CHECK(!game.particles[0].active); // expired
}

static void TestSpawnParticles(void) {
    ResetForTest();
    SpawnParticles((Vector2){50, 50}, 10, WHITE, BLACK, 100.0f, 5.0f, 1.0f, false);
    CHECK(CountActiveParticles() == 10);
    CHECK(game.nextFreeParticle == 10);

    // gravity particles get upward velocity reflected
    ResetForTest();
    SpawnParticles((Vector2){50, 50}, 1, WHITE, BLACK, 100.0f, 5.0f, 1.0f, true);
    CHECK(CountActiveParticles() == 1);

    // pool full -> spawn nothing
    for (int i = 0; i < MAX_PARTICLES; i++) game.particles[i].active = true;
    SpawnParticles((Vector2){50, 50}, 10, WHITE, BLACK, 100.0f, 5.0f, 1.0f, false);
    CHECK(CountActiveParticles() == MAX_PARTICLES);

    // wrap-around allocation across the two passes
    ResetForTest();
    for (int i = 2900; i < MAX_PARTICLES; i++) game.particles[i].active = true;
    game.nextFreeParticle = 2950;
    SpawnParticles((Vector2){50, 50}, 100, WHITE, BLACK, 100.0f, 5.0f, 1.0f, false);
    CHECK(CountActiveParticles() == 200);
    CHECK(game.nextFreeParticle == 100);
}

static int CountActiveFloatingTexts(void) {
    int n = 0;
    for (int i = 0; i < MAX_FLOATING_TEXT; i++) if (game.floatingTexts[i].active) n++;
    return n;
}

static void TestAddFloatingText(void) {
    ResetForTest();
    AddFloatingText((Vector2){100, 100}, "TEST", WHITE, true);
    FloatingText* ft = &game.floatingTexts[0];
    CHECK(ft->active);
    CHECK(ft->critical);
    CHECK(ft->lifetime == 2.0f);
    CHECK_STREQ(ft->text, "TEST");
    CHECK(ft->velocityY == -80.0f);

    AddFloatingTextFmt((Vector2){200, 200}, COLOR_GOLD, false, "+%dG", 42);
    ft = &game.floatingTexts[1];
    CHECK_STREQ(ft->text, "+42G");
    CHECK(!ft->critical);
    CHECK(ft->lifetime == 1.5f);

    // long formatted text truncates safely
    AddFloatingTextFmt((Vector2){0, 0}, WHITE, false, "%0100d", 7);
    CHECK(ft != NULL);

    // pool full -> no crash, no additions
    for (int i = 0; i < MAX_FLOATING_TEXT; i++) game.floatingTexts[i].active = true;
    AddFloatingText((Vector2){0, 0}, "X", WHITE, false);
    AddFloatingTextFmt((Vector2){0, 0}, WHITE, false, "Y");
    CHECK(CountActiveFloatingTexts() == MAX_FLOATING_TEXT);
}

static void TestScreenShake(void) {
    ResetForTest();
    ScreenShake(5.0f, 1.0f);
    CHECK(game.screenShakeIntensity == 5.0f);
    CHECK(game.screenShakeTime == 1.0f);

    ScreenShake(3.0f, 1.0f); // weaker -> ignored
    CHECK(game.screenShakeIntensity == 5.0f);

    ScreenShake(7.0f, 0.5f); // stronger -> replaces
    CHECK(game.screenShakeIntensity == 7.0f);
    CHECK(game.screenShakeTime == 0.5f);

    game.screenShakeTime = 0.0f; // expired -> any shake applies
    ScreenShake(2.0f, 0.2f);
    CHECK(game.screenShakeIntensity == 2.0f);
}

static void TestDrawVFX(void) {
    ResetForTest();
    SpawnParticles((Vector2){100, 100}, 5, WHITE, BLACK, 50.0f, 4.0f, 1.0f, false);
    AddFloatingText((Vector2){200, 200}, "FLOAT", COLOR_ENERGY, true);
    DrawVFX();
    CHECK(1);
}

void TestVFX(void) {
    TestUpdateVFX();
    TestSpawnParticles();
    TestAddFloatingText();
    TestScreenShake();
    TestDrawVFX();
}
