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
