// Test runner. Each module below lives in its own file and registers a void
// function here. Runs every suite, prints a summary, and returns non-zero on
// any failure so scripts can gate on it.
#include "test_util.h"
#include <stdio.h>

void TestUtilsAndMain(void);
void TestHero(void);
void TestTowers(void);
void TestEnemies(void);
void TestProjectiles(void);
void TestWavesUpdate(void);
void TestVFX(void);
void TestDrawUI(void);

int g_checks = 0;
int g_failures = 0;

int main(void) {
    InitGame(); // map + hero + camera; stubbed render texture is fine headless
    RebuildEnemyGrid();

    TestUtilsAndMain();
    TestHero();
    TestTowers();
    TestEnemies();
    TestProjectiles();
    TestWavesUpdate();
    TestVFX();
    TestDrawUI();

    printf("\n==========================================\n");
    printf("  Checks: %d   Failures: %d\n", g_checks, g_failures);
    if (g_failures == 0) printf("  ALL TESTS PASSED\n");
    else printf("  *** %d CHECK(S) FAILED ***\n", g_failures);
    printf("==========================================\n");
    return g_failures == 0 ? 0 : 1;
}
