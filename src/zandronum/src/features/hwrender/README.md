# features/hwrender — modern GL/Vulkan renderer port

Structure and rationale live in `docs/hwrender-portability-scope.md` (§11) and the staged execution
plan in `docs/hwrender-PLAN.md`. Short version:

- `computation/` — pure, 100%-tested units (no engine/GL/SDL headers), auto-discovered by
  `tests/CMakeLists.txt`. The seam math plus the one boundary that must be `fixed_t`-safe.
- feature-root `*.cpp/*.h` (later) — engine-coupled glue: the `IRenderBackend` seam, the vendored
  backend bring-up, and `scene_bridge` (walks our 64-bit `fixed_t` level data, emits into the
  backend). Compiled by the engine's explicit source list, never the test glob.
- vendored upstream backend lives OUTSIDE this dir, at `src/zandronum/rendering/` (UZDoom
  `common/rendering/` paths kept verbatim for cheap re-pulls) + `src/zandronum/ZVulkan/`.

**The `fixed_t` boundary is the whole risk.** We widened `fixed_t` to 64-bit 48.16. The vendored
backend is float-based, so the only crossing is `scene_bridge` → `computation/vertexconvert_compute`.
That conversion goes raw `int64` → `double` → `float` so the full 48.16 range survives; a naive
32-bit or `raw / 65536.0` path truncates past ~32k map units. Conversion is one-way (sim → pixels),
so netplay/demo determinism is untouched. See the `fixed64-widening` skill.

## How this fits the `features/` pattern

Per `features/README.md`, a feature folder holds ZandroX's **own** new files; vendored code and
in-place engine edits live elsewhere. This port splits accordingly:

- **Pure logic → `computation/`** (`*_compute.{h,cpp}` + `*_compute_test.cpp`): auto-globbed into
  both the test binary and the engine (`ZX_COMPUTE_SOURCES`), 100% covered. Safe to auto-glob
  because these never use `IMPLEMENT_CLASS`. This is the risky core (the `fixed_t` boundary, context
  and backend selection) made verifiable with zero CMake work.
- **New glue → this folder's root** (`hwrender_init.cpp`, `zx_texbridge.*`, `zx_video.*`,
  `scene_bridge.*`, `irenderbackend.h`): added to the `add_executable( zdoom … )` list **before
  `zzautozend.cpp`** (next to the `openal-sound`/`freeform-menu` entries), per the `creg` link-order
  rule — not via a trailing `target_sources`.
- **Vendored backend → NOT here.** `src/zandronum/rendering/` and `src/zandronum/ZVulkan/` are
  siblings of `src/` (like `zlib`), because `features/` is for our additions. Their `UPSTREAM.md` is
  the vendored analog of this README.

## In-place engine edits this feature requires (listed per the pattern)

The pattern says unavoidable in-place hooks stay put and get enumerated here. This port's are:

| File / area | Edit | Stage |
|---|---|---|
| `sdl/hardware.cpp` | `vid_hwrender` cvar | done |
| `wadsrc/static/menudef.txt` | OpenGL Options submenu + `RendererBackends` | done |
| `src/CMakeLists.txt` | add `rendering/**` sources; glue before `zzautozend.cpp`; `HAVE_VULKAN`/`ZVulkan` | 1/7 |
| `gl/textures/gl_material.*` | rename our `FMaterial` → `LegacyFMaterial` (yield the name) | 1 |
| `gl/scene/**` | retarget the BSP walker to emit into the vendored `FRenderState` (kept in place, not moved) | 4 |
| `gl/renderer/**`, `gl/shaders/**`, `wadsrc/static/shaders/glsl/` | deleted after parity | 6 |
| `sdl/*` (input/main/video) | rewritten in place to the SDL2 API | 2 |
| `menu/menu.cpp` (if a new menu class is registered) | register the type — `creg`-sensitive | 3 |

Keeping the retargeted `gl/scene` (14k lines) in place rather than moving it under `features/` is
deliberate: the pattern is for *new* files, and moving existing code would be churn — it's an
in-place edit, listed above.
