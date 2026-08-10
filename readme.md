# Aetherium Vanguard (Modular)

A raylib tower-defense game.

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
raylib flags via `pkg-config`, and produces `game.exe`. `-Release` strips all
debug info and symbol tables and enables assembly-level optimizations
(`-march=x86-64-v3`, `-funroll-loops`, `-fomit-frame-pointer`, `-flto`); this
is the build the GitHub release workflow ships.

## Run

```powershell
.\game.exe
```

## Tests

The game logic is covered by a headless unit-test suite in `tests/` that links
against a raylib stub (`tests/raylib_stub.c`) — no window or GPU required.
`tests/run_tests.sh` compiles every `src/*.c` module with gcov coverage
enabled, runs ~570 assertions across all modules (draw/UI tests assert on a
recorded draw-call log rather than just "no crash"), and prints a per-module
line coverage report:

```bash
bash tests/run_tests.sh
```

Current result: **~97% line coverage** across the 11 game modules (target
≥ 80%). The test binary and coverage artifacts are written under
`tests/build/` (gitignored).

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

## Scope

This restructuring preserves the original global-state design and gameplay
behavior. Known gameplay quirks from the original single-file version (e.g. the
double `RebuildEnemyGrid` call, level-up state flow) are intentionally kept
as-is and are out of scope for this change.
