// Test-only raylib runtime replacement. See raylib_stub.h.
//
// IMPORTANT: raymath is header-only; compile the whole test binary with
// -DRAYMATH_STATIC_INLINE so every math function becomes `static inline` and
// no external raylib library needs to be linked (the real libraylib would
// collide with these stubs and would require a window).

#include "raylib_stub.h"
#include "raymath.h"
#include <string.h>

// ---- input state ----
#define MAX_STUB_KEYS 512
#define MAX_STUB_MOUSE_BUTTONS 8

static bool s_keys_down[MAX_STUB_KEYS];
static bool s_keys_pressed[MAX_STUB_KEYS];
static bool s_mouse_down[MAX_STUB_MOUSE_BUTTONS];
static bool s_mouse_pressed[MAX_STUB_MOUSE_BUTTONS];
static Vector2 s_mouse_pos = { 0, 0 };
static float s_frame_time = 0.016f;

// ---- rng state (LCG, seeded deterministically) ----
static unsigned int s_rng_state = 0x9E3779B9u;
static bool s_rng_forced = false;
static int s_rng_forced_value = 0;

// ---- window state ----
static bool s_window_should_close = false;

// ======================= Test control =======================

void StubResetInput(void) {
    memset(s_keys_down, 0, sizeof(s_keys_down));
    memset(s_keys_pressed, 0, sizeof(s_keys_pressed));
    memset(s_mouse_down, 0, sizeof(s_mouse_down));
    memset(s_mouse_pressed, 0, sizeof(s_mouse_pressed));
    s_mouse_pos = (Vector2){ 0, 0 };
    s_frame_time = 0.016f;
    s_rng_forced = false;
}

void StubSetKeyDown(int key, bool down) {
    if (key >= 0 && key < MAX_STUB_KEYS) s_keys_down[key] = down;
}

void StubPressKey(int key) {
    if (key >= 0 && key < MAX_STUB_KEYS) {
        s_keys_down[key] = true;
        s_keys_pressed[key] = true;
    }
}

void StubClickMouse(int button) {
    if (button >= 0 && button < MAX_STUB_MOUSE_BUTTONS) {
        s_mouse_down[button] = true;
        s_mouse_pressed[button] = true;
    }
}

void StubSetMousePosition(float x, float y) { s_mouse_pos = (Vector2){ x, y }; }
void StubSetFrameTime(float dt) { s_frame_time = dt; }

void StubSetRandomValue(int value) {
    s_rng_forced = true;
    s_rng_forced_value = value;
}

void StubSetRandomSeed(unsigned int seed) { s_rng_state = seed; }

// ======================= raylib core =======================

void InitWindow(int width, int height, const char *title) {
    (void)width; (void)height; (void)title;
}

void SetTargetFPS(int fps) { (void)fps; }
void SetRandomSeed(unsigned int seed) { (void)seed; }
bool WindowShouldClose(void) { return s_window_should_close; }
float GetFrameTime(void) { return s_frame_time; }
void CloseWindow(void) {}

bool IsKeyDown(int key) {
    return (key >= 0 && key < MAX_STUB_KEYS) ? s_keys_down[key] : false;
}

bool IsKeyPressed(int key) {
    if (key >= 0 && key < MAX_STUB_KEYS && s_keys_pressed[key]) {
        s_keys_pressed[key] = false; // edge: consumed once
        return true;
    }
    return false;
}

bool IsMouseButtonPressed(int button) {
    if (button >= 0 && button < MAX_STUB_MOUSE_BUTTONS && s_mouse_pressed[button]) {
        s_mouse_pressed[button] = false; // edge: consumed once
        return true;
    }
    return false;
}

Vector2 GetMousePosition(void) { return s_mouse_pos; }

Vector2 GetScreenToWorld2D(Vector2 position, Camera2D camera) {
    Vector2 world = { 0 };
    if (camera.zoom == 0.0f) camera.zoom = 1.0f;
    if (camera.rotation == 0.0f) {
        world.x = (position.x - camera.offset.x) / camera.zoom + camera.target.x;
        world.y = (position.y - camera.offset.y) / camera.zoom + camera.target.y;
    } else {
        float cosR = cosf(camera.rotation * DEG2RAD);
        float sinR = sinf(camera.rotation * DEG2RAD);
        float dx = (position.x - camera.offset.x) / camera.zoom;
        float dy = (position.y - camera.offset.y) / camera.zoom;
        world.x = cosR * dx - sinR * dy + camera.target.x;
        world.y = sinR * dx + cosR * dy + camera.target.y;
    }
    return world;
}

bool CheckCollisionPointRec(Vector2 point, Rectangle rec) {
    return point.x >= rec.x && point.x <= rec.x + rec.width &&
           point.y >= rec.y && point.y <= rec.y + rec.height;
}

int GetRandomValue(int min, int max) {
    if (s_rng_forced) {
        s_rng_forced = false;
        int v = s_rng_forced_value;
        if (v < min) v = min;
        if (v > max) v = max;
        return v;
    }
    s_rng_state = s_rng_state * 1103515245u + 12345u;
    int range = max - min + 1;
    if (range <= 0) return min;
    return min + (int)((s_rng_state >> 16) % (unsigned int)range);
}

int MeasureText(const char *text, int fontSize) {
    if (!text) return 0;
    return (int)((float)strlen(text) * (float)fontSize * 0.6f);
}

RenderTexture2D LoadRenderTexture(int width, int height) {
    RenderTexture2D rt = { 0 };
    rt.id = 1;
    rt.texture.id = 1;
    rt.texture.width = width;
    rt.texture.height = height;
    rt.depth.id = 2;
    rt.depth.width = width;
    rt.depth.height = height;
    return rt;
}

void UnloadRenderTexture(RenderTexture2D target) { (void)target; }

bool IsRenderTextureValid(RenderTexture2D target) {
    return target.texture.id != 0 && target.texture.width > 0;
}

// ======================= draw-call log =======================

#define MAX_STUB_DRAW_CALLS 4096

static StubDrawCall s_draw_log[MAX_STUB_DRAW_CALLS];
static int s_draw_log_count = 0;

void StubResetDrawLog(void) { s_draw_log_count = 0; }

int StubDrawLogCount(void) { return s_draw_log_count; }

StubDrawCall StubDrawLogAt(int index) {
    StubDrawCall empty = {0};
    if (index < 0 || index >= s_draw_log_count) return empty;
    return s_draw_log[index];
}

static void StubLogDraw(StubDrawKind kind, float x, float y, float w, float h, Color color) {
    if (s_draw_log_count >= MAX_STUB_DRAW_CALLS) return;
    StubDrawCall *c = &s_draw_log[s_draw_log_count++];
    c->kind = kind;
    c->x = x; c->y = y; c->w = w; c->h = h;
    c->color = color;
    c->fontSize = 0;
    c->text[0] = '\0';
}

static void StubLogText(const char *text, int posX, int posY, int fontSize, Color color) {
    if (s_draw_log_count >= MAX_STUB_DRAW_CALLS) return;
    StubDrawCall *c = &s_draw_log[s_draw_log_count++];
    c->kind = STUB_DRAW_TEXT;
    c->x = (float)posX; c->y = (float)posY;
    c->w = (float)fontSize; c->h = 0.0f;
    c->color = color;
    c->fontSize = fontSize;
    c->text[0] = '\0';
    if (text) {
        strncpy(c->text, text, sizeof(c->text) - 1);
        c->text[sizeof(c->text) - 1] = '\0';
    }
}

// ======================= draw (no-ops + log) =======================

void BeginDrawing(void) {}
void EndDrawing(void) {}
void ClearBackground(Color color) { (void)color; }
void BeginMode2D(Camera2D camera) { (void)camera; }
void EndMode2D(void) {}
void BeginBlendMode(int mode) { (void)mode; }
void EndBlendMode(void) {}
void BeginTextureMode(RenderTexture2D target) { (void)target; }
void EndTextureMode(void) {}

void DrawText(const char *text, int posX, int posY, int fontSize, Color color) {
    StubLogText(text, posX, posY, fontSize, color);
}

void DrawRectangle(int posX, int posY, int width, int height, Color color) {
    StubLogDraw(STUB_DRAW_RECT, (float)posX, (float)posY, (float)width, (float)height, color);
}

void DrawRectangleRec(Rectangle rec, Color color) { (void)rec; (void)color; }

void DrawRectanglePro(Rectangle rec, Vector2 origin, float rotation, Color color) {
    (void)rec; (void)origin; (void)rotation; (void)color;
}

void DrawRectangleLinesEx(Rectangle rec, float lineThick, Color color) {
    (void)rec; (void)lineThick; (void)color;
}

void DrawRectangleRounded(Rectangle rec, float roundness, int segments, Color color) {
    (void)rec; (void)roundness; (void)segments; (void)color;
}

void DrawRectangleRoundedLinesEx(Rectangle rec, float roundness, int segments,
                                 float lineThick, Color color) {
    (void)rec; (void)roundness; (void)segments; (void)lineThick; (void)color;
}

void DrawRectangleGradientV(int posX, int posY, int width, int height,
                            Color color1, Color color2) {
    (void)posX; (void)posY; (void)width; (void)height; (void)color1; (void)color2;
}

void DrawCircleV(Vector2 center, float radius, Color color) {
    StubLogDraw(STUB_DRAW_CIRCLE_FILL, center.x, center.y, radius, 0.0f, color);
}

void DrawCircleLines(int centerX, int centerY, float radius, Color color) {
    StubLogDraw(STUB_DRAW_CIRCLE_LINE, (float)centerX, (float)centerY, radius, 0.0f, color);
}

void DrawPoly(Vector2 center, int sides, float radius, float rotation, Color color) {
    (void)center; (void)sides; (void)radius; (void)rotation; (void)color;
}

void DrawLine(int startPosX, int startPosY, int endPosX, int endPosY, Color color) {
    (void)startPosX; (void)startPosY; (void)endPosX; (void)endPosY; (void)color;
}

void DrawLineEx(Vector2 startPos, Vector2 endPos, float thick, Color color) {
    (void)startPos; (void)endPos; (void)thick; (void)color;
}

void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest,
                    Vector2 origin, float rotation, Color tint) {
    (void)texture; (void)source; (void)dest; (void)origin; (void)rotation; (void)tint;
}

// ======================= color helpers =======================

Color Fade(Color color, float alpha) {
    color.a = (unsigned char)((float)color.a * alpha);
    return color;
}

Color ColorTint(Color color, Color tint) {
    color.r = (unsigned char)(((int)color.r * (int)tint.r) / 255);
    color.g = (unsigned char)(((int)color.g * (int)tint.g) / 255);
    color.b = (unsigned char)(((int)color.b * (int)tint.b) / 255);
    return color;
}

Color ColorBrightness(Color color, float factor) {
    if (factor < 0.0f) {
        float m = 1.0f + factor;
        color.r = (unsigned char)((float)color.r * m);
        color.g = (unsigned char)((float)color.g * m);
        color.b = (unsigned char)((float)color.b * m);
    } else {
        color.r = (unsigned char)((255 - color.r) * factor + color.r);
        color.g = (unsigned char)((255 - color.g) * factor + color.g);
        color.b = (unsigned char)((255 - color.b) * factor + color.b);
    }
    return color;
}

Color ColorAlphaBlend(Color dst, Color src, Color tint) {
    (void)tint;
    float a = (float)src.a / 255.0f;
    Color out;
    out.r = (unsigned char)((float)src.r * a + (float)dst.r * (1.0f - a));
    out.g = (unsigned char)((float)src.g * a + (float)dst.g * (1.0f - a));
    out.b = (unsigned char)((float)src.b * a + (float)dst.b * (1.0f - a));
    out.a = dst.a;
    return out;
}

Color ColorLerp(Color color1, Color color2, float factor) {
    color1.r = (unsigned char)((float)color1.r + ((float)color2.r - color1.r) * factor);
    color1.g = (unsigned char)((float)color1.g + ((float)color2.g - color1.g) * factor);
    color1.b = (unsigned char)((float)color1.b + ((float)color2.b - color1.b) * factor);
    color1.a = (unsigned char)((float)color1.a + ((float)color2.a - color1.a) * factor);
    return color1;
}

// ======================= rlgl (no-ops) =======================

void rlBegin(int mode) { (void)mode; }
void rlEnd(void) {}
void rlVertex2f(float x, float y) { (void)x; (void)y; }
void rlColor4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    (void)r; (void)g; (void)b; (void)a;
}
void rlSetTexture(unsigned int id) { (void)id; }
