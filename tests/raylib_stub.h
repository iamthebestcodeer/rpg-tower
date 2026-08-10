#ifndef RAYLIB_STUB_H
#define RAYLIB_STUB_H

// Test-only replacement for the raylib runtime. The game's logic modules call a
// handful of raylib functions (input, RNG, drawing). This stub provides
// controllable, side-effect-free implementations so the game logic can be
// exercised headlessly (no window, no GPU) under gcov.

#include "raylib.h"
#include "rlgl.h"
#include <stdbool.h>

// ---- Test control API ----
void StubResetInput(void);                  // clear all input/timing/RNG-override state
void StubSetKeyDown(int key, bool down);    // polled state (IsKeyDown)
void StubPressKey(int key);                 // one-shot edge (IsKeyPressed), also sets down
void StubClickMouse(int button);            // one-shot edge (IsMouseButtonPressed)
void StubSetMousePosition(float x, float y);// GetMousePosition / GetScreenToWorld2D
void StubSetFrameTime(float dt);            // GetFrameTime
void StubSetRandomValue(int value);         // next GetRandomValue returns `value` (clamped), once
void StubSetRandomSeed(unsigned int seed);  // seed the deterministic LCG

// ---- Draw-call log ----
// The draw functions are no-ops, but they record every call so tests can
// assert on what *would* have been rendered (health bars, overlays, range
// circles) instead of only checking that the draw path doesn't crash.
// The log is bounded; when full, further calls are dropped (count stays at
// the cap). Tests should StubResetDrawLog() right before the draw they assert
// on and use the helpers in test_util.h (StubFindText, StubFindCircle).
typedef enum {
    STUB_DRAW_TEXT,        // DrawText:              x,y = pos, w = fontSize, text
    STUB_DRAW_RECT,        // DrawRectangle:         x,y,w,h
    STUB_DRAW_CIRCLE_FILL, // DrawCircleV:           x,y = center, w = radius
    STUB_DRAW_CIRCLE_LINE  // DrawCircleLines:       x,y = center, w = radius
} StubDrawKind;

typedef struct {
    StubDrawKind kind;
    float x, y, w, h;
    Color color;
    int fontSize;
    char text[64];
} StubDrawCall;

void StubResetDrawLog(void);               // clear the log
int  StubDrawLogCount(void);               // entries recorded so far
StubDrawCall StubDrawLogAt(int i);         // copy of entry i (all-zero if out of range)

// ---- rlgl activity log ----
// The stub's rlgl calls are no-ops, but rlBegin/rlVertex2f activity is
// recorded (the batched-circle path emits all geometry through them). Counts
// are cumulative since the last StubResetRlglLog().
void StubResetRlglLog(void);   // clear begin/vertex counts
int  StubRlBeginCount(void);   // rlBegin calls since last reset
int  StubRlVertexCount(void);  // rlVertex2f calls since last reset

#endif // RAYLIB_STUB_H
