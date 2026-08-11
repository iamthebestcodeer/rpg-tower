// Tests for the --bench harness in main.c: drive the frame counter headless
// through the BENCH_UNIT_TEST seam and assert that every accumulator counts
// exactly the measured frames - never the warm-up.
#include "test_util.h"

// Run `totalFrames` measured frames after a full warm-up with a known
// per-frame phase cost and a fixed world load, then verify the sums.
static void TestBenchmarkMeasuredWindow(void) {
    const int totalFrames = 50;
    const double loopMs = 5.0, updateMs = 1.0, drawMs = 2.0;

    ResetForTest();
    // A known, static world load so the entity sums are predictable.
    SpawnParticles((Vector2){100, 100}, 3, WHITE, BLACK, 50.0f, 4.0f, 1.0f, false);
    SpawnEnemyAt(ENEMY_BASIC, 220, 300);
    SpawnEnemyAt(ENEMY_TANK, 280, 300);
    for (int i = 0; i < 4; i++) game.projectiles[i].active = true;
    AddFloatingText((Vector2){150, 150}, "T", WHITE, false);

    const int warmup = BenchTestWarmupFrames();
    BenchTestBegin(totalFrames);

    // Warm-up frames: not a single tick may land in any accumulator, the
    // sample buffer stays untouched, and the draw trace stays off.
    for (int i = 0; i < warmup - 1; i++) {
        CHECK(BenchTestTick(loopMs, updateMs, drawMs) == false);
        CHECK(BenchTestLoopMs() == 0.0);
        CHECK(BenchTestUpdateMs() == 0.0);
        CHECK(BenchTestDrawMs() == 0.0);
        CHECK(BenchTestSampleAt(0) == 0.0);
        CHECK(BenchTestTraceEnabled() == false);
    }
    // The final warm-up frame arms the draw trace for the measured window
    // but still accumulates nothing itself.
    CHECK(BenchTestTick(loopMs, updateMs, drawMs) == false);
    CHECK(BenchTestLoopMs() == 0.0);
    CHECK(BenchTestUpdateMs() == 0.0);
    CHECK(BenchTestDrawMs() == 0.0);
    CHECK(BenchTestTraceEnabled() == true);
    CHECK(BenchTestParticleLoad() == 0);
    CHECK(BenchTestEnemyLoad() == 0);
    CHECK(BenchTestProjectileLoad() == 0);
    CHECK(BenchTestTextLoad() == 0);

    // Measured window: exactly `totalFrames` frames land in every
    // accumulator, then the harness reports the run complete.
    int measured = 0;
    bool done = false;
    while (!done) {
        measured++;
        done = BenchTestTick(loopMs, updateMs, drawMs);
    }
    CHECK(measured == totalFrames);
    CHECK_NEAR(BenchTestLoopMs(), loopMs * totalFrames, 1e-9);
    CHECK_NEAR(BenchTestUpdateMs(), updateMs * totalFrames, 1e-9);
    CHECK_NEAR(BenchTestDrawMs(), drawMs * totalFrames, 1e-9);
    CHECK(BenchTestParticleLoad() == 3L * totalFrames);
    CHECK(BenchTestEnemyLoad() == 2L * totalFrames);
    CHECK(BenchTestProjectileLoad() == 4L * totalFrames);
    CHECK(BenchTestTextLoad() == 1L * totalFrames);
    // Every sample slot holds a real (positive) frame time.
    for (int i = 0; i < totalFrames; i++)
        CHECK(BenchTestSampleAt(i) > 0.0);

    BenchTestClearPixelProbe(); // drop the frame-10 probe request
}

// A single measured frame: the window shrinks to one frame after the warm-up
// and the harness still finishes cleanly.
static void TestBenchmarkSingleMeasuredFrame(void) {
    const double loopMs = 1.0, updateMs = 0.5, drawMs = 0.25;
    const int warmup = BenchTestWarmupFrames();

    BenchTestBegin(1);
    for (int i = 0; i < warmup; i++)
        CHECK(BenchTestTick(loopMs, updateMs, drawMs) == false);
    CHECK(BenchTestTick(loopMs, updateMs, drawMs) == true); // the one measured frame
    CHECK_NEAR(BenchTestLoopMs(), loopMs, 1e-9);
    CHECK_NEAR(BenchTestUpdateMs(), updateMs, 1e-9);
    CHECK_NEAR(BenchTestDrawMs(), drawMs, 1e-9);
    CHECK(BenchTestSampleAt(0) > 0.0);

    BenchTestClearPixelProbe();
}

void TestBench(void) {
    TestBenchmarkMeasuredWindow();
    TestBenchmarkSingleMeasuredFrame();
}
