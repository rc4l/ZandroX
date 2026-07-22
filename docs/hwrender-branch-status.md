# hwrender port — branch status (`feature/hwrender-port`)

Honest state of the modern GL/Vulkan renderer port on this branch. Companion to
`docs/hwrender-portability-scope.md` (scoping/architecture), `docs/hwrender-integration-plan.md`
(the staged integration), and `docs/hwrender-PLAN.md` (the prior-attempt log this branch mines).
**WIP — do not merge.**

## Where the code came from

The prior attempt (`archive/hwrender-port` in the sibling clone, 79 commits, reached mid-P3 with
MAP01 rendering through a core context) was 117 commits stale and **predates the `fixed_t` 64-bit
widening**. This branch transplanted it onto current `main` via per-file 3-way merges and
reconciled it with the widening and with this branch's own foundations. The verbatim UZDoom vendor
(`rendering/`, `ZVulkan/`, pinned `7bfbf61`) is kept as the re-pull reference; the build compiles the
archive's *adapted* copy at `features/hwrender/backend/`.

## Compiles and passes (verified this session)

- **Full engine build green** (`mac_compile.sh` exit 0, app + pk3): the ported UZDoom GL backend +
  shims + glue, the 2D drawer, the legacy-scene capture hooks, **native SDL2** (sdl12-compat no
  longer linked; `sdlvideo.cpp`/`SDLMain.m` deleted), and the core-context request path
  (`vid_hwrender` → `zx::ResolveBackend` → `ComputeGLContextRequests`).
- **184/184 tests green**, 100% coverage gate on all `computation/` units (13 units: the archive's
  ten + this branch's 64-bit-safe `vertexconvert`, `glcontext`, `backendselect` — the archive's
  32-bit versions of the latter two were superseded, not merged).
- **`fixed_t` reconciliation:** the strong `zx::Fixed` type caught upstream's stale
  `typedef int32_t fixed_t` in the backend shim at compile time (backend has zero `fixed_t`
  consumers — pure collision, now defers to `basictypes.h`); the `textures.h` conflict was resolved
  keeping the fixed64 casts; every applied hunk audited for `FRACBITS`/narrowing patterns.
- **pk3 verified:** 30 `shaders/hwrender/` lumps + the OpenGL Options menu are in `zandronum.pk3`.
- **Vulkan wiring:** `HAVE_VULKAN` (default OFF — CI untouched) + `add_subdirectory(ZVulkan)` + link;
  `mac_compile.sh` auto-detects the MoltenVK/vulkan-headers/vulkan-loader brew kegs and opts in,
  or warns and builds without. `vid_hwrender 2` resolves to core GL until a Vulkan device exists.

## NOT verified (the honest gap)

- **Runtime.** Nothing on this branch has been run. The archive attempt rendered MAP01 through the
  core path at ~25% A/B difference with known defects (sprite slice, 2D gaps — see the PLAN log);
  this branch carries that code but its behaviour on the 64-bit base is unconfirmed. The Zandronum
  MCP is registered for this project but connects next session — first action then: launch MAP01
  under `vid_hwrender 0` (legacy must be unregressed, especially SDL2 input/video), then `1`, and
  A/B both against the pre-port captures (`tools/ab_render.py`).
- **Keyboard/mouse input** under native SDL2 (scancode path) needs a human at the keyboard.
- The Vulkan *backend* (`rendering/vulkan`) is not compiled — that is plan P5, behind the same seam;
  ZVulkan-the-library is the adjacency this branch wires.

## Next actions, in order

1. MCP runtime A/B: `vid_hwrender 0` regression check, then `1`, on the fixed scene set.
2. Chase the archive's known core-path defects (sprite slice; 2D `Dim`/`FlatFill` hooks) — now with
   the F2DDrawer vendored and compiling.
3. P4 legacy deletion once parity; P5 Vulkan device bring-up behind the seam.
