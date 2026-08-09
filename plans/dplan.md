# Plan: Convert `game.c` into a full C project

**Goal:** Turn the single-file raylib game `game.c` (2,774 lines) into a structured,
multi-file C project **with minimal code changes** — pure restructuring, no behavior changes.

## Decided requirements

| Question | Decision |
|---|---|
| Build system | `gcc` (MSYS2 UCRT64), single gcc command via `src/build.ps1` (PowerShell) |
| Raylib | via `pkg-config` (`--cflags` / `--libs`), resolved by the script |
| Granularity | Fine-grained split (~12 `.c` files + 1 header) |
| Layout | Everything lives in `src/` |
| Warnings | Build with `-Wall -Wextra`; **fix all warnings** (behavior-neutral only) |
| Scope | **No bug fixing** — functions move verbatim |

## Layout (all inside `src/`)

```
src/
  game.h
  main.c
  update.c
  hero.c
  towers.c
  enemies.c
  projectiles.c
  waves.c
  draw.c
  ui.c
  vfx.c
  utils.c
  build.ps1
  README.md
```

## Module responsibilities

| File | Contents |
|---|---|
| `game.h` | Includes, macros, enums, structures, `extern GameData game`, prototypes, `GetEnemyFromPair` |
| `main.c` | `GameData game`, `main`, initialization/reset/map functions |
| `update.c` | Top-level update/state/input/environment functions |
| `hero.c` | Hero skills, movement, attacks, XP, level-up |
| `towers.c` | Tower updating, placement, sale, upgrades, stats, tower XP |
| `enemies.c` | Spatial grid, enemy update/spawn/death, damage and status processing |
| `projectiles.c` | Projectile creation, updating, impacts, damage/effects |
| `waves.c` | Wave scheduling and spawning |
| `draw.c` | Top-level game, map, entity, enemy, tower, hero, and projectile drawing |
| `ui.c` | Sidebar, menus, inspector, tooltips, buttons |
| `vfx.c` | VFX updating/drawing, particles, floating text, screen shake |
| `utils.c` | Geometry, lookup, tower metadata, colors, costs, angle interpolation |

Putting `DrawVFX` in `vfx.c` keeps VFX behavior together.

The **only** required code edit: `static GameData game = {0};` → `extern GameData game;`
in `game.h`, with `GameData game = {0};` defined once in `main.c`.
Everything else moves **verbatim** from the warning-clean baseline.

## Execution phases

### Phase 1: Establish the baseline

Before moving anything:

1. Confirm repository state:
   ```
   git status
   ```
2. Confirm Raylib configuration:
   ```
   pkg-config --modversion raylib
   pkg-config --cflags raylib
   pkg-config --libs raylib
   ```
3. Compile the original file using the exact final standard and warning flags:
   ```
   gcc -std=c99 -Wall -Wextra -O2 \
       $(pkg-config --cflags raylib) \
       game.c \
       $(pkg-config --libs raylib) -lm \
       -o game-baseline.exe
   ```
4. Record all warnings, if any.
5. Launch the baseline executable and perform a short smoke test.

**Do not** fix gameplay bugs discovered during this test.

> **Amendment (verification limits):** I can launch the binary and confirm it starts
> without crashing (timeout + kill), but I cannot drive keyboard/mouse input into a
> native window — interactive verification is a human checklist (see Phase 9).

### Phase 2: Make warning-only corrections

If the original single-file build emits warnings:

- Fix warnings before splitting where practical.
- Restrict fixes to behavior-neutral changes, such as:
  - `(void)param;` for intentionally unused parameters
  - Correct format specifiers
  - Explicit casts where runtime conversion already occurred
  - Removing genuinely unused local variables
  - Using `(void)` in no-argument declarations/definitions if needed
- Rebuild until this succeeds without warnings:
  ```
  gcc -std=c99 -Wall -Wextra -O2 ...
  ```
  This creates a clean source baseline so the extraction itself can remain nearly verbatim.
- **Do not** enable additional warning groups as mandatory requirements during this refactor.

> **Amendment:** after Phase 1, delete `game-baseline.exe` (or add it to `.gitignore`).
> `-std=c99` is the intended pin; if raylib 5.5's headers object, fall back to `gnu99`.

### Phase 3: Create `src/game.h`

Move the following into `game.h`:

- Raylib and standard-library includes
- Configuration macros
- Enums
- Structures
- Function prototypes

Add an include guard:

```c
#ifndef GAME_H
#define GAME_H

/* includes, constants, types and declarations */

#endif
```

Replace:

```c
static GameData game = {0};
```

with:

```c
extern GameData game;
```

Place the full inline definition after `GameData` and the `extern` declaration:

```c
static inline Enemy *GetEnemyFromPair(int index, int id)
{
    if (index >= 0 && index < MAX_ENEMIES) {
        Enemy *e = &game.enemies[index];
        if (e->active && e->id == id) return e;
    }
    return NULL;
}
```

Remove the separate incomplete declaration:

```c
static inline Enemy* GetEnemyFromPair(int index, int id);
```

The header should contain one complete definition, not both a distant declaration and definition.

### Phase 4: Extract incrementally

Each `.c` file begins with:

```c
#include "game.h"
```

Perform extraction one module at a time. Recommended order:

```
main.c
utils.c
vfx.c
waves.c
hero.c
towers.c
enemies.c
projectiles.c
update.c
draw.c
ui.c
```

Define the global once in `main.c`:

```c
#include "game.h"

GameData game = {0};
```

After each extraction:

- Compile all current modules
- Fix missing declarations or duplicate symbols immediately
- Confirm no function remains defined in two files
- Confirm no function was omitted

Function bodies should otherwise be copied unchanged from the warning-clean baseline.

### Phase 5: Add the build script (`build.ps1`)

A single gcc command compiles every source and links in one step — no Makefile, no `.o`
files. `build.ps1` just wraps it so you don't retype the long command:

```powershell
# build.ps1 - Build Aetherium Vanguard
# Usage:
#   .\build.ps1          # build game.exe
#   .\build.ps1 -Clean   # remove build artifacts

param(
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$Target = 'game.exe'

if ($Clean) {
    Remove-Item -Force $Target -ErrorAction SilentlyContinue
    Write-Host 'Cleaned.' -ForegroundColor Green
    exit 0
}

# Requires gcc + pkg-config (MSYS2 UCRT64) on PATH
$cflags = (pkg-config --cflags raylib).Split(' ')
$libs   = (pkg-config --libs raylib).Split(' ')

$gccArgs = @('-std=c99', '-Wall', '-Wextra', '-O2') +
           $cflags +
           (Get-ChildItem -Filter '*.c').FullName +
           $libs + @('-lm', '-o', $Target)

& gcc @gccArgs

if ($LASTEXITCODE -eq 0) {
    Write-Host "Built $Target" -ForegroundColor Green
}
exit $LASTEXITCODE
```

Notes:

- The equivalent raw command (also valid from the MSYS2 UCRT64 shell):
  ```
  gcc -std=c99 -Wall -Wextra -O2 \
      $(pkg-config --cflags raylib) \
      *.c \
      $(pkg-config --libs raylib) -lm \
      -o game.exe
  ```
- `gcc` compiles all `.c` files to temporary objects and links them — no `.o` files
  remain on disk, so `-Clean` only removes `game.exe`.
- `gcc` and `pkg-config` must be on `PATH` (e.g., `C:\msys64\ucrt64\bin`) in the
  PowerShell session, or run from an MSYS2 UCRT64 shell.

### Phase 6: Add documentation

`src/README.md` should include:

- **Requirements**: MSYS2 UCRT64, UCRT64 GCC + pkg-config on PATH, Raylib development package
- **Build**: `cd src; .\build.ps1`
- **Run**: `.\game.exe`
- **Clean rebuild**: `.\build.ps1 -Clean` then `.\build.ps1`
- **Controls** (already shown by the game):
  WASD: move · Q: dash · E: burst · Space: attack · 1–4: select tower · N: next wave ·
  G: grid · P: pause · Escape/right-click: cancel selection
- **Scope statement**: the split preserves the original global-state design and gameplay behavior.

### Phase 7: Remove the original file

Only after the modular project builds successfully:

```
git rm game.c
```

The original remains recoverable through Git history. **Do not** delete it before the
extracted project has compiled successfully.

> Note: `git rm` stages the deletion; nothing is committed unless requested.

### Phase 8: Clean-build verification

```
cd src
.\build.ps1 -Clean
.\build.ps1
```

Acceptance criteria:

- All expected source files are compiled
- Linking succeeds
- No `-Wall` or `-Wextra` warnings
- Exactly one definition of `game`
- No duplicate-symbol errors
- No unresolved-symbol errors
- `game.exe` launches successfully

> Phase 8 is the real warning gate — re-verify there, since a few new warnings can
> appear post-split that didn't exist in the single file.

### Phase 9: Behavior smoke test

Compare the modular build against the baseline for:

- Title screen and starting the game
- Hero movement
- Hero basic attack
- Dash and burst
- Tower selection and placement
- Invalid placement handling
- Tower targeting and firing
- Tower XP and level-up
- Tower specialization upgrades
- Selling towers
- Wave spawning and completion
- Pause and resume
- Hero level-up UI
- Game-over and restart
- Particles, floating text, and screen shake
- Grid display and day/night rendering

Known or discovered gameplay bugs should be **recorded but not fixed** in this
restructuring change.

> **Amendment:** this is a human checklist — run it in the native window. I can only
> confirm the binary launches without crashing.

## Out of scope (explicitly)

- No bug fixes (e.g., the double `RebuildEnemyGrid` call, level-up state flow)
- No gameplay/balance changes
- No new features
