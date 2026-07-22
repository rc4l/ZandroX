# hwrender port — branch status (`feature/hwrender-port`)

Honest state of the modern GL/Vulkan renderer port on this branch. Companion to
`docs/hwrender-portability-scope.md` (the scoping/architecture) and `docs/hwrender-PLAN.md` (the
staged execution plan). **This branch is WIP and must not be merged** — it is the foundation and the
vendored source, not a working modern renderer yet.

## Done and verified (tests green, 100% coverage gate)

| Piece | What | Verification |
|---|---|---|
| `features/hwrender/computation/vertexconvert_compute` | The `fixed_t` boundary: raw 64-bit fixed → float, safe past ±32k map units | 5 tests incl. a past-INT32_MAX regression guard; 100% cov |
| `features/hwrender/computation/glcontext_compute` | Core (4.1→4.0→3.3) / compat (3.0→2.1) context request chain (plan P2) | 3 tests; 100% cov |
| `features/hwrender/computation/backendselect_compute` | `vid_hwrender` → backend, with clamp/fallback/restart rules shared by menu + init | 5 tests; 100% cov |

All 124 tests pass (`cmake --build build-tests && ctest`); the coverage gate reports 100% on every
new `computation/` unit. Baseline before this branch was 111 tests.

## Done, vendored verbatim (compiles into nothing yet — dormant)

| Piece | Source | Notes |
|---|---|---|
| `src/zandronum/rendering/` | UZDoom `common/rendering/` @ `7bfbf61` (149 files) | GL + GLES + Vulkan + hwrenderer backend. Zero game/ZScript coupling. `rendering/UPSTREAM.md` |
| `src/zandronum/ZVulkan/` | UZDoom `libraries/ZVulkan` @ `7bfbf61` (159 files) | Self-contained: volk + vk_mem_alloc + glslang. No system Vulkan SDK. `ZVulkan/UPSTREAM.md` |

Not yet added to any CMake source list — bringing them up (the P1 adaptation surface: `zx_video`,
`zx_texbridge`, the filesystem/printf shims, etc.) is the next large step and is where the plan's P1
effort actually lives.

## Menu changes (see build note below before trusting)

- `vid_hwrender` cvar (`sdl/hardware.cpp`), archived, restart-only, **default 0 so behaviour is
  unchanged** until the backend is wired.
- `menudef.txt`: a new `OpenGLOptions` submenu (linked from Display Options) that also gives the
  already-working GL cvars (`gl_render_precise`, `gl_seamless`, `gl_mirror_envmap`,
  `gl_plane_reflection`, `r_mirror_recursions`) a menu home they previously lacked, plus the
  `RendererBackends` selector. Core-GL/Vulkan entries are labelled experimental.

## What is NOT done (the honest gap)

The renderer is **not** updated at runtime yet. Remaining, in plan order:

1. **P1 finish** — wire the vendored backend into the build and stand up the adapters so it compiles
   live but dormant (the bulk of the integration work).
2. **P2** — SDL 1.2 → SDL2 migration for the core/Vulkan context (`glcontext_compute` is ready).
3. **P3** — `scene_bridge`: retarget our fixed-point BSP walker to emit into the vendored
   `FRenderState` (this is where `vertexconvert_compute` gets used for real).
4. **P4** — delete the legacy fixed-function path.
5. **P5** — bring up Vulkan behind the same seam.
6. **Menus** — hook `Dim`/`FlatFill` under the new backend (§9), add the borderless tri-state (§10).

Runtime A/B verification (MCP `launch_instance` → `screenshot`) has **not** been run on this branch;
nothing draws through the ported path here yet.

## Build note

Both build paths pass on this branch:

- `build-tests` (GoogleTest): **124 tests green, 100% coverage** on every new `computation/` unit.
- Full engine build via `mac_compile.sh`: **exit 0** — `ZandroX.app` and `zandronum.pk3` rebuilt.
  This confirms the `vid_hwrender` cvar and the `menudef.txt` OpenGL Options submenu compile, link,
  and package. The engine also compiles the `features/hwrender/computation/*.cpp` into itself (they
  were made C++14-clean after the build flagged an `inline constexpr` C++17 extension warning).

The vendored `rendering/`/`ZVulkan` trees are **not** in the engine's source list yet, so they do
not affect the engine build — bringing them up is the P1 step above.

**Not yet done:** runtime verification. The new OpenGL Options submenu compiles and packages, but has
not been confirmed on screen via the MCP (`launch_instance` → menu nav → `screenshot`). Nothing draws
through the ported backend on this branch.
