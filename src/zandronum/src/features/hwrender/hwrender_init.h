// [rc4l] Startup/shutdown of the ported UZDoom GL backend, called from the platform video layer once a core-profile context is current.
#ifndef ZX_HWRENDER_INIT_H
#define ZX_HWRENDER_INIT_H

// [rc4l] DrawParms is nested in DCanvas, so the full declaration is needed rather than a forward.
#include "v_video.h"

namespace hwrender
{

// [rc4l] Recorded by the platform video layer once a context exists; only the ported backend can run in a core profile.
void SetCoreProfile(bool core);
bool IsCoreProfile();

// [rc4l] Compiles the ported shader set against the current context; false if it fails.
// Must run after the GL entry points are loaded, or every gl* call is a null pointer.
bool InitPortedShaders();

void ShutdownPortedShaders();

// [rc4l] Draws one frame through the ported/core path. The legacy renderer is immediate-mode and
// cannot run in a core context, so under core it is bypassed entirely and this owns the frame.
void RenderCoreFrame(int width, int height);

// [rc4l] Plan-A scene bridge: called right before ProcessScene under core. Clears the frame, loads
// the captured camera into the ported viewpoint buffer, and arms the emission hooks to draw inline
// through the ported FRenderState (per-draw state -- no captured globals, no deferred replay).
void BeginSceneDraw(int viewwidth, int viewheight);

// [rc4l] Queues one 2D texture draw for the current core frame; replayed by RenderCoreFrame.
// [rc4l] Hands one 2D texture draw to UZDoom's F2DDrawer, replacing the bespoke queue.
void Add2DTexture(FTexture *img, DCanvas::DrawParms &parms);

// [rc4l] translation is the legacy GL translation id (0 = none; -GLTranslationPalette::GetIndex()
// for a DrawParms remap) -- fonts are paletted and draw rainbow garbage without it.
void Queue2DTexture(FTexture *img, float x, float y, float w, float h, unsigned int rgba,
	int translation = 0);

// [rc4l] Same, but with explicit texture coordinates; the player weapon needs its own UV window.
void Queue2DTextureUV(FTexture *img, float x, float y, float w, float h,
	float u0, float v0, float u1, float v1, unsigned int rgba, int translation = 0);

// [rc4l] Same, but tinted with the lit surface colour gl_SetColor last recorded. The player weapon
// is a world object drawn in screen space, so it takes the sector's lighting like everything else.
void Queue2DTextureLit(FTexture *img, float x, float y, float w, float h,
	float u0, float v0, float u1, float v1);

// [rc4l] Camera state for the current scene, captured where the legacy path would set the fixed-function matrices.
void SetSceneCamera(float roll, float pitch, float yaw, float camX, float camY, float camZ,
	bool mirror, bool planeMirror, float fov, float ratio, float fovRatio);

// [rc4l] Queues one scene quad (4 corners, Doom coordinates) for the current core frame.
struct SceneVertex { float x, y, z, u, v; };
void QueueSceneQuad(FTexture *tex, const SceneVertex corners[4], unsigned int rgba);

// [rc4l] Records the lit surface colour the legacy path would have passed to glColor4f, which core
// removes. Scene geometry queued afterwards is tinted with it.
void SetSurfaceColor(float r, float g, float b, float a);

// [rc4l] Fade colour and raw density from gl_SetFog; the premultiply happens at upload.
void SetFogParams(float r, float g, float b, float density, bool enabled);

// [rc4l] Queues one convex triangle fan (flats are subsector fans of arbitrary length).
// [rc4l] patchTex binds the patch texture variant (sprites: legacy binds BindPatch and computes
// sprite-window UVs against it -- binding the world variant against those UVs slices the sprite);
// flats keep the world variant, which tiles. translation is the legacy translation id.
void QueueSceneFan(FTexture *tex, const SceneVertex *verts, int count, unsigned int rgba, bool translucent = false,
	int translation = 0, bool patchTex = false);

} // namespace hwrender

#endif // ZX_HWRENDER_INIT_H
