# Aetherium Vanguard (Modular)

A raylib tower-defense game split from the original single-file `game.c` into a
structured multi-file C project. The split is a pure restructuring: no gameplay
or behavior changes were introduced.

## Requirements

- **MSYS2 UCRT64** environment
- **UCRT64 GCC** (`gcc`) and **pkg-config** on `PATH` (e.g. `C:\msys64\ucrt64\bin`)
- **Raylib development package** (`pacman -S mingw-w64-ucrt-x86_64-raylib`)

## Build

From `src/`:

```powershell
.\build.ps1          # dev build
.\build.ps1 -Release # release build (stripped + assembly-optimized)
```

Each runs a single `gcc` command with `-std=c99 -Wall -Wextra -O3`, resolving
raylib flags via `pkg-config`, and produces `game.exe`.

`-Release` additionally strips all debug info and symbol tables (`-s`, no
`.eh_frame` unwind tables) and enables assembly-level optimizations:
`-march=x86-64-v3` (AVX2/FMA/BMI2), `-funroll-loops`, `-fomit-frame-pointer`,
and `-flto`. `-march=native` is deliberately avoided so the release binary never
carries AVX-512 code from the GitHub runner that could crash on older CPUs.
This is the build used by the GitHub release workflow.

## Run

```powershell
.\game.exe
```

## Tests

The game logic is covered by a headless unit-test suite in `../tests/` that
links against a raylib stub (`../tests/raylib_stub.c`) — no window or GPU
required. `../tests/run_tests.sh` compiles every `src/*.c` module with gcov
coverage enabled, runs ~570 assertions across all modules (draw/UI tests assert on a
recorded draw-call log rather than just "no crash"), and prints a per-module
line coverage report:

```bash
bash ../tests/run_tests.sh
```

Current result: **~97% line coverage** across the 11 game modules (target
≥ 80%). Test artifacts are written under `../tests/build/` (gitignored).

## Clean rebuild

```powershell
.\build.ps1 -Clean
.\build.ps1
```

`-Clean` removes `game.exe` (gcc compiles and links in one step, leaving no
intermediate `.o` files).

## Controls

| Input | Action |
|---|---|
| WASD | Move hero |
| Q | Dash |
| E | Aether Burst |
| Space | Attack |
| 1–4 | Select tower to place |
| N | Start next wave |
| P | Pause / resume |
| Escape / right-click | Cancel selection |

## Module layout

| File | Contents |
|---|---|
| `game.h` | Includes, macros, enums, structs, `extern GameData game`, prototypes |
| `main.c` | `main`, `GameData game`, initialization/reset/map functions |
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

## Performance notes

- **Frame cap:** `SetTargetFPS(60)` prevents the render loop from spinning
  unthrottled (which would peg a CPU core).
- **No MSAA:** the `FLAG_MSAA_4X_HINT` flag was removed - it multisampled the
  whole framebuffer at 4x on every frame for little visible gain on flat-color
  shapes. Re-enable it in `main()` if you prefer smoother edges.
- **Idle updates skipped:** `UpdateEnvironment`/`UpdateVFX` only run in live
  states (`GS_PLAYING`, `GS_LEVEL_UP_HERO`, `GS_GAME_OVER`), so the title
  screen does no wasted work and pause genuinely freezes particles/floaties.
- **Deterministic draw path:** Tesla sparkles are time-based instead of calling
  `GetRandomValue` per tower per frame.
- **Batched circle rendering:** particles, projectiles, and enemy bodies are
  emitted as precomputed triangle fans inside a single `rlBegin` block instead
  of per-circle `DrawCircleV` calls. Raylib's circle draws recompute
  `sinf()`/`cosf()` for every segment of every circle every frame, which with
  `MAX_PARTICLES` alive is hundreds of thousands of trig calls per frame; the
  batched path costs zero per-frame trig.
- **Spatial grid:** enemy queries (tower targeting, AoE, hero attacks) go
  through the spatial grid, rebuilt before tower queries and again before
  projectile impacts (enemy movement invalidates cell membership mid-frame).

## Scope

This restructuring preserves the original global-state design and gameplay
behavior. Known gameplay quirks from the original single-file version (e.g. the
double `RebuildEnemyGrid` call, level-up state flow) are intentionally kept
as-is and are out of scope for this change.
