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
.\build.ps1
```

This runs a single `gcc` command with `-std=c99 -Wall -Wextra -O2`, resolving
raylib flags via `pkg-config`, and produces `game.exe`.

## Run

```powershell
.\game.exe
```

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
| G | Toggle grid |
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
- **Spatial grid:** enemy queries (tower targeting, AoE, hero attacks) go
  through the spatial grid, rebuilt before tower queries and again before
  projectile impacts (enemy movement invalidates cell membership mid-frame).

## Scope

This restructuring preserves the original global-state design and gameplay
behavior. Known gameplay quirks from the original single-file version (e.g. the
double `RebuildEnemyGrid` call, level-up state flow) are intentionally kept
as-is and are out of scope for this change.
