# Plan: Migrate to raylib 6.0

**Goal:** Move the modular `src/` tower-defense project from raylib 5.5 (currently
installed) to raylib 6.0 with **minimal source changes**, verified by a clean
`-Wall -Wextra` build and a smoke test. Expected outcome: **zero game-code edits**
— the migration is a dependency upgrade plus verification, because this game's
raylib surface area is small and 6.0 did not break any of it.

## Current state (verified)

| Item | Value |
|---|---|
| Installed raylib | `5.5.0` (`pkg-config --modversion raylib`) |
| Package source | MSYS2 UCRT64, `mingw-w64-ucrt-x86_64-raylib` (per `src/README.md`) |
| Build | `src/build.ps1` → single `gcc -std=c99 -Wall -Wextra -O2` + `pkg-config` flags |
| MSYS2 raylib 6.0 | **Already available**: `mingw-w64-ucrt-x86_64-raylib 6.0-1` |
| Includes | `game.h` includes `raylib.h` + `raymath.h` only (no `core.h`/`shapes.h`/`text.h`) |
| Headers layout in 6.0 | Consolidated back to a single `raylib.h` (module `.h` files removed) — no include change needed |

## raylib 6.0 breaking changes vs. this game

raylib 6.0's breaking changes are flagged `-WARNING-` in the CHANGELOG. Full list
reviewed; here is the mapping against this codebase:

| 6.0 change (CHANGELOG) | Used here? | Impact |
|---|---|---|
| `GetRandomValue()` modulo-bias fix | **Yes** (~25 call sites) | **Behavioral only** — signature unchanged; RNG sequences differ from 5.5 for the same seed |
| Fullscreen modes / HighDPI redesign | No | None (fixed 1280×800 window, no fullscreen, no HighDPI flag) |
| `DrawCircleGradient()` center → `Vector2` | No (game uses `DrawCircleV`, `DrawCircleLines`) | None |
| `DrawRectangleRounded()` auto-segment count | Partial | Game passes explicit `16` segments → unaffected; change only kicks in at `segments == 0` |
| `DrawLine()` / `DrawRectangleLines()` pixel-offset fixes | Yes | **Behavioral, cosmetic** — 1px lines may land a half-pixel differently |
| `TextInsert()` → static buffer, `LoadFontData()` new param | No | None |
| `Encode/DecodeDataBase64()`, `File*()` return values, `ImageDraw*()` redesigns, `SetSoundPan`/`SetMusicPan` range, `DrawCapsule` param order, shader API renames, `DrawModelPoints*` removal | No | None |
| `MOUSE_LEFT_BUTTON`/`MOUSE_RIGHT_BUTTON` names | Yes | Still provided as compat `#define`s in 6.0 (confirmed in header) |

**API surface audit** — every function the game calls is signature-compatible with
6.0 (verified against the 6.0 `raylib.h` for core/window/drawing, and unchanged
elsewhere): `InitWindow`, `CloseWindow`, `WindowShouldClose`, `SetTargetFPS`,
`GetFrameTime`, `SetRandomSeed`, `GetRandomValue`, `GetMousePosition`,
`GetScreenToWorld2D`, `IsKeyPressed`, `IsMouseButtonPressed`, `LoadRenderTexture`,
`UnloadRenderTexture`, `BeginTextureMode`, `EndTextureMode`, `DrawTexturePro`,
`DrawRectangle`, `DrawRectangleRec`, `DrawRectangleGradientV`,
`DrawRectangleRounded`, `DrawRectangleRoundedLinesEx`, `DrawLine`, `DrawLineEx`,
`DrawCircleV`, `DrawCircleLines`, `DrawText`, `MeasureText`, `Fade`, `ColorLerp`,
`ColorAlphaBlend`, `ColorBrightness`, `ColorTint`, plus the `Vector2`/`Rectangle`/
`Color`/`Camera2D`/`RenderTexture2D` types and `raymath.h` functions
(`Vector2Subtract`, `Vector2Scale`, `Vector2Normalize`, `Vector2Distance`, …).

> Conclusion: **no compile-time migration work is expected.** The plan below still
> gates on that assumption with a contingency checklist, in case the MSYS2 package
> ships a config/GLFW quirk that the header audit can't predict.

## Decided requirements

| Question | Decision |
|---|---|
| Upgrade path | MSYS2 UCRT64 package upgrade: `pacman -Syu` then `pacman -S mingw-w64-ucrt-x86_64-raylib` |
| Source changes | **None expected**; if the build breaks, see contingency table (Phase 4) |
| Standard/flags | Keep `-std=c99 -Wall -Wextra -O2` exactly as `build.ps1` pins |
| Scope | Dependency upgrade + verification only — no refactor, no bug fixes, no new raylib 6.0 features |
| Rollback | Downgrade the package (`pacman -U` cached 5.5 pkg, or re-run `pacman -Syu` after MSYS2 resolves) |

## Execution phases

### Phase 1: Preflight (before touching the system)

1. Confirm clean repo state:
   ```
   git status
   ```
2. Record the current baseline:
   ```
   pkg-config --modversion raylib
   cd src && ./build.ps1
   ```
   Keep the freshly built `game.exe` as the pre-migration binary to compare against.
3. Confirm the target package version is available:
   ```
   pacman -Si mingw-w64-ucrt-x86_64-raylib
   ```
   (Expected: `Version: 6.0-1` or newer.)

> No code changes in this phase. If the workspace has uncommitted work, stop and
> check ownership before proceeding.

### Phase 2: Upgrade the package

From an MSYS2 UCRT64 shell:

```
pacman -Syu
pacman -S --needed mingw-w64-ucrt-x86_64-raylib
```

Notes:

- `pacman -Syu` first — MSYS2 requires a full system/database update before
  installing, and it may update the toolchain in the same pass.
- If `pacman -Syu` itself upgrades raylib (it will if the version is newer than
  installed), the explicit `-S` line is a no-op safeguard.
- Verify the new version through the same channel the build uses:
  ```
  pkg-config --modversion raylib     # expect 6.0 (or 6.x)
  pkg-config --cflags raylib
  pkg-config --libs raylib
  ```
  If `pkg-config` still reports 5.5, the package DB path (`PKG_CONFIG_PATH`) or the
  MSYS2 environment being used is wrong — investigate before continuing.

### Phase 3: Build against 6.0

```
cd src
./build.ps1 -Clean
./build.ps1
```

Expected outcome:

- Compiles and links **with zero source edits**.
- Zero `-Wall`/`-Wextra` warnings (same gate as the original refactor).
- `game.exe` is produced.

### Phase 4: Contingency — only if the build breaks

If the 6.0 headers/libraries reject the current source, work down this list and
record whatever was needed (do **not** silently change behavior):

| Symptom (expected error) | Fix |
|---|---|
| Any `DrawCircleGradient`-style signature mismatch | Not possible in this codebase (not used) — sanity-check the audit instead |
| `MOUSE_LEFT_BUTTON` undeclared | 6.0 still defines it; if a future 6.x drops the alias, replace with `MOUSE_BUTTON_LEFT` (single name change in `update.c`) |
| `-std=c99` header objections | Same fallback the 5.5 refactor allowed: try `-std=gnu99` and note it in `build.ps1` |
| Linker `undefined reference` | Re-check `pkg-config --libs raylib`; confirm the package actually upgraded (Phase 2) |
| New warnings from raylib macros (e.g. `-Wpedantic`-style) | Keep `-Wall -Wextra` only; do not broaden the warning gate during migration |

### Phase 5: Behavioral smoke test

Launch `game.exe` (timeout + kill is enough for automated verification; a human
plays for the rest) and check:

- Window opens at 1280×800 and the title screen renders.
- Game starts; hero moves/attacks; towers place, fire, level up; waves spawn.
- Pause, game-over restart, level-up panel, and the static map render texture
  (`mapRT`) all render correctly.
- No console warnings/errors during startup.

**Expect minor visual/roll deltas, not bugs:**

- `GetRandomValue` draws different numbers than 5.5 for the same seed — spawns,
  crits, particle angles and screen shake will look "different" run to run. This is
  **not** a regression (the game seeds with `time(NULL)`, so it was never
  deterministic). Do not "fix" it.
- 1px `DrawLine` UI separators may shift a half-pixel from the 6.0 pixel-offset
  fix. Cosmetic; accept unless clearly worse.

### Phase 6: Documentation

- Update `src/README.md` requirements line to state raylib 6.0 (it currently just
  names the package, so this is a one-word/one-line touch-up if a version is
  mentioned anywhere).
- Optionally add a "Raylib 6.0" note under the frame-cap/performance section since
  the previous plan doc references 5.5 (`plans/dplan.md` is historical — leave it).

### Phase 7: Acceptance criteria

- [ ] `pkg-config --modversion raylib` reports 6.0 (or newer 6.x)
- [ ] `./build.ps1 -Clean && ./build.ps1` succeeds with **no source edits**
- [ ] Zero `-Wall`/`-Wextra` warnings under `-std=c99`
- [ ] `game.exe` launches and the smoke checklist (Phase 5) passes
- [ ] No behavioral fixes were made to compensate for the upgrade
- [ ] Working tree contains no stray artifacts beyond the rebuilt `game.exe`

## Out of scope (explicitly)

- Adopting new raylib 6.0 features (software renderer `rlsw`, memory/Win32
  backends, new file-system API, `DrawCircleLinesEx`, borderless fullscreen).
- Any gameplay, balance, or bug-fix work (repo convention: record, don't fix).
- Build-system changes (keep `build.ps1` + `pkg-config`).
- Downgrade scripts or vendoring raylib into the repo.
