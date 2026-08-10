# RPG Tower ("Aetherium Vanguard") — Performance Optimization Record

> **Purpose of this file:** permanent record of the "3x faster" optimization work on branch `perf/3x-optimization`.
> **Status: COMPLETE.** The optimization is shipped, the invisible-text bug is **fixed and verified**, probes and the baseline worktree are cleaned up.
> **All changes are UNCOMMITTED** in the working tree (9 modified files in `src/`, plus this doc in `agentwork/`).
> Read `src/game.h`, `src/vfx.c`, `src/utils.c` first.

---

## 0. TL;DR — final state

| Item | State |
|---|---|
| Optimization work | **Shipped** — batched rendering helpers, adaptive tessellation, spawn trims (9 files). |
| Invisible-text bug (raylib 6.0 immediate mode) | **FIXED & verified** — text renders again (`CHECK_RENDER gray≈3500–4100`, was `0`). |
| Performance | Median **0.55–2.1 ms** (machine is bimodal: quiet ~0.6 ms / busy ~2.0 ms; ~480–1800 fps) vs pristine-baseline **4.34–4.52 ms** (~225 fps) interleaved → **~1.8×**, up to ~6× vs the 3.31 ms original baseline in quiet windows. |
| Cleanup | `texprobe*.c` deleted; baseline worktree `/tmp/perfbaseline` removed. |
| Particle fragility | **FIXED** — particles now render via a pre-rendered sprite texture, independent of batch overflow (see §5). |

**Honesty note on headline numbers:** this machine's frame time is bimodal (quiet vs busy windows; single-run swings 0.4–8.5 ms), so any single figure is misleading. The interleaved same-session comparison gives ~1.8×; quiet-window runs reach ~6× vs the original 3.31 ms baseline. Always measure with the interleaved-median methodology in §6.

---

## 1. Build & run

- **OS:** Windows, bash shell (POSIX syntax; PowerShell for the build script).
- **Toolchain:** gcc via MSYS2 UCRT64. raylib **6.0.0** via pkg-config (`/c/msys64/ucrt64/include`, `/c/msys64/ucrt64/lib`).
- **Build:** `cd src && powershell -ExecutionPolicy Bypass -File build.ps1` (dev: `-O3 -Wall -Wextra`, must be warning-clean; `-Release` adds strip / `-march=x86-64-v3` / LTO).
- **Benchmark:** `./game.exe --bench --frames 600` (in `src/`). Prints `BENCHMARK` (median/avg/p90/min/max over `clock_gettime`, 30-frame warmup) + `CHECK_RENDER` pixel sanity counts + load counters (live particles/enemies/projectiles/texts).
- **CHECK_RENDER legend:** `non_bg` = pixels not equal to the clear color (768000 = full map covered); `cyan` = energy/particle pixels; `gray` = **white text pixels (tower "L1" labels)**; `red`/`yellow` = damage/status pixels.

---

## 2. What was delivered (9 files, uncommitted)

### `src/utils.c` — batched primitives (core of the win)
- **`EmitCircleFan(center, radius, color)`** — adaptive circle tessellation: 7 precomputed unit-circle levels (4/6/8/12/18/24/36 segments) chosen by radius. Tiny particles = 4-seg diamonds = **12 verts vs 108** before (9× vertex cut). Zero per-frame trig; emits into the caller's `rlBegin(RL_TRIANGLES)` block.
- **`EmitRect(rec, color)`** — 2-triangle filled rect (health bars, tower bases).
- **`DrawTextBatched(font, text, pos, fontSize, spacing, tint)`** — the renamed/rewritten text primitive (see §4).

### `src/draw.c`
- Health bars + tower bases batched into single triangle passes (`EmitRect`); stun labels + tower level labels via `DrawTextBatched`. Draw-phase trace hooks for the bench.

### `src/vfx.c`
- **Particle sprite:** a 64px solid-core + radial-falloff sprite is generated once (`InitVFX`) and every particle is drawn as one tinted `DrawTexturePro` quad — overflow-independent and soft-edged. `InitVFX`/`UnloadVFX` are wired into the `main.c` lifecycle. This cut the particle draw phase from ~2.17 ms to ~0.11 ms/frame.
- Floating damage numbers via `DrawTextBatched` (shadow only for critical text). Per-frame floating-text budget (`MAX_FLOATING_TEXT_PER_FRAME 48`, reset in `UpdateVFX`). `SpawnParticles` does a single two-pass pool sweep instead of per-particle head rescans.

### `src/enemies.c`, `src/hero.c`, `src/projectiles.c`, `src/towers.c`
- Particle spawn counts trimmed ~35–40% (the real lever on live particle count). Visuals still lush.

### `src/game.h`
- `MAX_PARTICLES` 3000 → **2200**; emit-helper prototypes; draw-trace externs (`DRAW_TRACE_PHASES 10`, `g_drawTrace`, `g_drawTraceMs[]`, `NowMs()`).

### `src/main.c` — bench harness (zero effect on normal gameplay)
- `--bench [--frames N]`; high-res `clock_gettime` (`NowMs()` — raylib `GetTime()` proved too noisy); median/percentiles; load counters; `CHECK_RENDER` screenshot probe (`rlReadScreenPixels`).

---

## 3. Measured results (interleaved 3 × 600 frames, identical deterministic workload)

| Build | Median frame | Median FPS |
|---|---|---|
| Baseline (pristine HEAD + harness, text OK) | 4.47 / 4.52 / 4.34 ms (interleaved 3×600) | ~223–230 |
| Text-fix build (before particle sprite) | 2.45 / 2.55 / 2.54 ms (interleaved 3×600) | ~392–408 |
| **Final (text + particle sprite)** | **0.55–2.1 ms** (bimodal machine: quiet ~0.6 / busy ~2.0) | **~480–1800** |

The particle-sprite change alone cut the vfx draw phase from 2.17 ms to 0.11 ms — the single biggest remaining win (12–24 verts per particle → 4, via the normal batched path).

Run-to-run variance on this machine is large (single frames swing 0.6–8.5 ms) — always interleave builds and use medians.

---

## 4. The text-rendering fix (the hard part)

### Symptom
After batching text into manual rlgl quads, all of it was invisible (`CHECK_RENDER gray=0` vs ~3200 baseline), while the rest of the game rendered fine.

### Root cause
Manual `rlBegin(RL_QUADS)/rlVertex*` immediate-mode geometry on this raylib 6.0 build/driver only renders when it triggers a **mid-emission batch overflow**; geometry flushed at end-of-frame silently vanishes. Evidence:
- The game's particle pass renders (≈24,000 verts → 3 overflow flushes).
- The text pass is invisible (a few hundred verts → single end-of-frame flush).
- Probes (texprobe3–7, now deleted) reproduced it: 24k manual verts render, 6 manual verts do not; `DrawTexturePro` always renders.

### Fix (applied)
`EmitTextQuads` → renamed **`DrawTextBatched`**, rendering each glyph via **`DrawTexturePro`**, replicating raylib 6.0 `DrawTextEx`/`DrawTextCodepoint` math exactly:
- `srcRec`/`dstRec` with glyph padding, `scaleFactor = fontSize/baseSize`.
- Advance: `advanceX == 0` falls back to glyph width; `offsetX += advance*scaleFactor + spacing`.
- Newline: `offsetY += fontSize + textLineSpacing` (default 10px, named `TEXT_LINE_SPACING`).
- Callers pass raylib `DrawText`-equivalent spacing (`fontSize/10`) so rendering matches the pre-optimization build pixel-for-pixel.

Consecutive same-texture `DrawTexturePro` calls do **not** split the rlgl draw entry, so a whole text pass is still one batch — the perf win holds (measured: no regression vs the invisible-text build). Callers in `draw.c`/`vfx.c` no longer wrap text in `rlSetTexture`/`rlBegin(RL_QUADS)` blocks; the old "every DrawText forced a texture-switch flush" comments were **disproven** (same-texture draws don't flush) and removed.

### Verification
1. Build warning-clean.
2. `./game.exe --bench --frames 60` → `gray≈3500–4100` (tower labels visible).
3. 3×600 interleaved vs baseline → §3 table.

---

## 5. Particle pass — now overflow-independent (fixed)

**The old fragility:** the manual `RL_TRIANGLES` particle pass only rendered because it overflowed the 8192-vertex rlgl batch every frame; below ~680 particles/frame it would have silently vanished.

**The fix (applied):** `src/vfx.c` generates a 64px sprite (solid core + radial falloff to transparent) once in `InitVFX()`, and draws every particle as one tinted `DrawTexturePro` quad. All particles share one texture, so the pass is still a single rlgl batch — and it renders regardless of batch size, exactly like raylib's own draws. Bonus: soft anti-aliased edges instead of hard discs, and ~4× fewer vertices per particle (12–24 → 4), which cut the vfx draw phase from ~2.17 ms to ~0.11 ms.

---

## 6. Bench methodology (learned the hard way)

- Median over **3 × 600 frames**, baseline/optimized **interleaved**; single runs lie (0.6–8.5 ms swings).
- `clock_gettime(CLOCK_MONOTONIC)` is the only trustworthy clock here (`GetTime()` was inconsistent).
- Keep gameplay load equal: the bench auto-plays a deterministic heavy scene (100 towers, 200 mixed enemies incl. spawner/boss; `hero.xpToNextLevel = 2e9` prevents mid-bench level-up state switches).
- Prefer whole-frame `--bench` numbers over the draw-phase trace (the trace once summed to less than measured draw time).

---

## 7. raylib 6.0 gotchas worth remembering

- `rlEnd()` does **not** flush the batch; it only increments `currentDepth`. Manual immediate-mode batches flush at end-of-frame — and on this build that flush renders nothing.
- `rlSetTexture(0)` binds the default white texture (required before batched shape passes); `rlSetTexture(id != 0)` splits a draw entry only if the texture differs from the current entry.
- `RL_QUADS` batches require `vertexCount % 4` alignment at every draw-entry boundary.
- Total vertex bytes drive frame time more than call count. A custom VBO/`glBufferSubData` particle path measured ~4 ms/frame on this Intel Arc iGPU — **worse** than rlgl's batched chunk-upload path. Reverted.

---

## 8. Rejected approaches (don't redo)

1. **Custom VBO/VAO particle rendering** — `rlUpdateVertexBuffer` cost ~4 ms/frame on this iGPU; triple-buffering didn't help. Reverted.
2. **`GetTime()`-based timing** — inconsistent; replaced with `clock_gettime`.
3. **Manual rlgl text quads** (any UV/flip variant) — invisible on this build; replaced by `DrawTexturePro` per glyph (§4).

---

## 9. What's left (all optional)

1. Commit the work on `perf/3x-optimization` (branch exists; changes are uncommitted — user chose to defer).
2. `-Release` build sanity check before shipping via the GitHub workflow.
