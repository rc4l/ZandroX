// [rc4l] The 2D/HUD draw path over the hwrender seam (Phase 2, first migrated subsystem). It owns
// the base shader + a dynamic vertex buffer and turns "draw this textured, tinted rect" into
// buffer upload + state + Draw on any IRenderBackend (GL now, Vulkan later). The geometry/matrix
// math comes from the tested computation/ helpers; this class is the thin glue, verified at runtime
// via the MCP. Built but not yet called by the engine 2D code until the Phase 1 Core-context switch.
#ifndef ZX_HWRENDER_RENDERER2D_H
#define ZX_HWRENDER_RENDERER2D_H

#include "features/hwrender/irenderbackend.h"
#include "features/hwrender/computation/matrix_compute.h"

namespace hwrender
{

class Renderer2D
{
public:
	// [rc4l] Compile the base shader + allocate the dynamic quad buffer. False if the shader fails.
	bool Init(IRenderBackend *backend);
	void Shutdown();

	// [rc4l] Set the orthographic projection for a screenW x screenH target (origin top-left).
	void Begin(int screenW, int screenH);

	// [rc4l] Set an arbitrary view-projection and enable depth testing, for scene geometry.
	void BeginScene(const Mat4 &mvp);

	// [rc4l] Fade colour and premultiplied density for the scene pass; alpha 0 disables fog.
	void SetFog(float r, float g, float b, float density, bool enabled);

	// [rc4l] Draw one scene quad as two triangles; corners are in world space.
	void DrawSceneQuad(TextureHandle tex, const struct SceneVertex *corners,
	                   unsigned char r, unsigned char g, unsigned char b, unsigned char a);

	// [rc4l] Draw a convex triangle fan; corners are in world space.
	void DrawSceneFan(TextureHandle tex, const struct SceneVertex *verts, int count,
	                  unsigned char r, unsigned char g, unsigned char b, unsigned char a,
	                  bool translucent);

	// [rc4l] Draw one textured, tinted rect with the given blend mode.
	void DrawTexturedQuad(TextureHandle tex, float x, float y, float w, float h,
	                      float u0, float v0, float u1, float v1,
	                      unsigned char r, unsigned char g, unsigned char b, unsigned char a,
	                      BlendMode blend);

private:
	IRenderBackend *mBackend = nullptr;
	ShaderHandle mShader = ShaderHandle::None;
	BufferHandle mVbo = BufferHandle::None;
	int mMvpLoc = -1;
	int mFogColorLoc = -1;
	int mFogDensityLoc = -1;
	int mTexLoc = -1;
	int mAlphaLoc = -1;
};

} // namespace hwrender

#endif // ZX_HWRENDER_RENDERER2D_H
