# features/hwrender/backend — vendored UZDoom render backend

**Status: vendored, NOT yet wired into the build.** These sources are copied from UZDoom and are
being adapted incrementally; nothing here is compiled until each piece builds cleanly. See
`../PLAN.md`.

## Why these files and not UZDoom's renderer as a whole

Measured coupling decided the split:

| Component | Game-model coupling | Verdict |
|---|---|---|
| `common/rendering/gl/` | `AActor` 0, `sector_t` 0, `seg_t` 0, `FGameTexture` 0, `DVector` 0, `PClass`/`VMFunction` 0 | vendored |
| `common/rendering/hwrenderer/` | `AActor` 0, `sector_t` 0, `PClass` 0; `FGameTexture` in 3 files | vendored (adapt 3) |
| `common/rendering/gl_load/` | none | vendored |
| `wadsrc/static/shaders/glsl/` | assets | vendored |
| `rendering/hwrenderer/scene/` | `sector_t` 17 files, `AActor` 9, `FGameTexture` 6 | **NOT taken** |

The float-sim / ZScript / `FGameTexture` coupling that would break Zandronum's deterministic netcode
lives entirely in the **scene** layer. Zandronum's own `gl/scene/*` keeps walking our fixed-point
level data and will be retargeted to emit into this backend.

## Provenance & licensing

Copied from `UZDoom/src/common/rendering/{gl,hwrenderer,gl_load}` and
`UZDoom/wadsrc/static/shaders/glsl/`. Upstream SPDX headers are preserved verbatim and are all
GPL-3.0-compatible: **GPL-3.0-or-later** (99 files), **BSD-3-Clause** (27), **Zlib** (16). ZandroX
ships GPL-3.0, so this does not regress GPL compliance. Do not strip the upstream headers.

## Adaptation notes (the integration work)

The backend has no game-model dependencies, but it does use ZDoom-lineage *infrastructure* APIs that
Zandronum has in older form. These are the shim points:

- `filesystem.h` (UZDoom `FileSystem`) → our `Wads` / `FWadCollection` — used to load shader lumps.
- `v_video.h` (`DFrameBuffer`/`DCanvas`) → our older equivalents. The largest adaptation.
- `printf.h` → our `Printf` (`c_console.h`); `c_cvars.h`, `cmdlib.h`, `version.h` → ours.
- `matrix.h` (`VSMatrix`) → pure math, takes as-is.
- `m_png.h`, `i_time.h`, `r_videoscale.h` → our equivalents.
- **GL loader:** point at our existing `gl/system/gl_system.h` rather than vendored `gl_load`, so we
  don't define GL entry points twice while the legacy renderer still links. Extend our loader
  (`gl/api/gl_api.h` + `gl/system/gl_interface.cpp`) with any missing modern entry points.

Per `features/README.md`, any pure logic we write during adaptation belongs in
`../computation/` as tested `Compute*` helpers, not inline in the glue.
