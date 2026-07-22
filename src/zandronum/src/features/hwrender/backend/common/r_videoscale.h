// [rc4l] Shim for UZDoom's r_videoscale.h. Their 2D emitter asks whether the output is a scaled
// viewport; ours always renders at the framebuffer's own size, so it never is.
#pragma once

inline bool ViewportIsScaled43() { return false; }
inline int ViewportScaledWidth(int width, int) { return width; }
inline int ViewportScaledHeight(int, int height) { return height; }
