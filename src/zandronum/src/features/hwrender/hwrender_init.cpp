// [rc4l] Brings the ported UZDoom GL backend up once a core-profile context exists. Phase 3 starts
// here: the shader manager is the first ported subsystem to actually run against the live context,
// and getting its 30 GLSL lumps to compile is the prerequisite for anything drawing through it.
#include "features/hwrender/hwrender_init.h"
#include "features/hwrender/irenderbackend.h"
#include "features/hwrender/renderer2d.h"
#include "features/hwrender/computation/sceneview_compute.h"
#include "features/hwrender/computation/backendprereq_compute.h"
#include "gl/textures/gl_material.h"
#include "v_video.h"
#include <vector>
#include <stdio.h>

#ifndef NO_GL // [rc4l] The ported backend is absent from the dedicated-server (NO_GL) build.

#include "doomtype.h"
#include "v_text.h"
#include "c_cvars.h"
// [rc4l] Path-qualified: the GL entry points must be declared before gl_shader.h, and unqualified names here would resolve against our own gl/ tree.
#include "features/hwrender/backend/gl_load/gl_system.h"
#include "features/hwrender/backend/gl/gl_shader.h"
#include "features/hwrender/backend/2d/v_2ddrawer.h"
#include "features/hwrender/backend/gl/gl_renderstate.h"
#include "features/hwrender/backend/gl/gl_renderer.h"
#include "features/hwrender/backend/gl/gl_samplers.h"
#include "features/hwrender/backend/gl/gl_debug.h"

// [rc4l] Defined in the ported hw_draw2d.cpp.
void Draw2D(F2DDrawer *drawer, FRenderState &state);

// [rc4l] Lives at global scope in the ported gl_debug.cpp.
EXTERN_CVAR(Int, gl_debug_level)
#include "features/hwrender/backend/gl/gl_buffers.h"
#include "features/hwrender/backend/common/zx_video.h"
#include "features/hwrender/backend/hwrenderer/data/hw_lightbuffer.h"
#include "features/hwrender/backend/hwrenderer/data/hw_bonebuffer.h"
#include "features/hwrender/backend/hwrenderer/data/hw_viewpointbuffer.h"
#include "features/hwrender/backend/hwrenderer/data/flatvertices.h"

namespace hwrender
{

namespace
{
// [rc4l] Owned here rather than by the ported FGLRenderer, which we have not stood up yet.
OpenGLRenderer::FShaderManager *s_shaderManager = nullptr;
bool s_coreProfile = false;

// [rc4l] The ported code reaches through the zx_screen global for buffer creation and capability
// queries. Their OpenGLFrameBuffer supplies this, but compiling it drags in the renderer and the
// whole postprocess chain, so this covers just what the shader and buffer paths actually touch.
class ZXMinimalFrameBuffer : public ZXFrameBuffer
{
public:
	ZXMinimalFrameBuffer(int width, int height) : ZXFrameBuffer(width, height) {}

	void InitializeState() override {}

	// [rc4l] Window-geometry queries the shader path never reaches; the real values live on our own framebuffer.
	bool IsFullscreen() override { return false; }
	int GetClientWidth() override { return GetWidth(); }
	int GetClientHeight() override { return GetHeight(); }

	IDataBuffer *CreateDataBuffer(int bindingpoint, bool ssbo, bool needsresize) override
	{
		return new OpenGLRenderer::GLDataBuffer(bindingpoint, ssbo);
	}

	// [rc4l] The base returns nullptr, which their 2D drawer dereferences without checking; its
	// vertex buffer is created once at startup, so a null here spins the first frame.
	IVertexBuffer *CreateVertexBuffer() override
	{
		return new OpenGLRenderer::GLVertexBuffer();
	}

	IIndexBuffer *CreateIndexBuffer() override
	{
		return new OpenGLRenderer::GLIndexBuffer();
	}
};
}

void SetCoreProfile(bool core) { s_coreProfile = core; }

bool IsCoreProfile() { return s_coreProfile; }

bool InitPortedShaders()
{
	if (s_shaderManager != nullptr) return true;

	// [rc4l] The ported backend resolves GL through its own loader, separate from our gl_LoadExtensions.
	// Upstream does this in their OpenGLFrameBuffer, which we do not compile, so without this every
	// backend GL call goes through an unloaded pointer.
	if (ogl_LoadFunctions() == ogl_LOAD_FAILED)
	{
		Printf(TEXTCOLOR_RED "hwrender: could not load the ported backend's GL entry points\n");
		return false;
	}

	// [rc4l] FShader::Load dereferences zx_screen->mLights/mBones with no null check -- upstream only
	// asserts them and asserts compile out in release -- so these must exist before the first compile.
	if (zx_screen == nullptr)
	{
		zx_screen = new ZXMinimalFrameBuffer(1, 1);
		zx_screen->mLights = new FLightBuffer();
		zx_screen->mBones = new BoneBuffer();
		// [rc4l] Their 2D pass calls mViewpoints->Set2D unconditionally.
		zx_screen->mViewpoints = new HWViewpointBuffer(1);
		// [rc4l] Their 2D pass restores this vertex buffer at the end of every frame.
		zx_screen->mVertexData = new FFlatVertexBuffer(1, 1);
	}

	if (zx_screen->mLights == nullptr || zx_screen->mBones == nullptr)
	{
		Printf(TEXTCOLOR_ORANGE "hwrender: light/bone buffers unavailable; shaders not compiled\n");
		return false;
	}

	// [rc4l] The texture uploader reaches through GLRenderer->mSamplerManager on every bind, so it
	// must exist before anything draws. Their gl_renderer.cpp is not compiled, so we build the
	// pieces we actually use rather than the whole renderer.
	if (OpenGLRenderer::GLRenderer == nullptr)
	{
		// [rc4l] Their constructor takes the framebuffer; nothing we drive touches it.
		OpenGLRenderer::GLRenderer = new OpenGLRenderer::FGLRenderer(nullptr);
		OpenGLRenderer::GLRenderer->mSamplerManager = new OpenGLRenderer::FSamplerManager();
	}

	// [rc4l] Install the ported KHR_debug callback where it exists. It does not on macOS -- KHR_debug
	// is GL 4.3 and Apple caps at 4.1 -- so this is a no-op here and only helps on Windows/Linux.
	{
		static OpenGLRenderer::FGLDebug debug;
		if (gl_debug_level == 0) gl_debug_level = 2;
		debug.Update();
	}

	s_shaderManager = new OpenGLRenderer::FShaderManager();
	OpenGLRenderer::GLRenderer->mShaderManager = s_shaderManager;

	// [rc4l] The manager compiles one shader per call and reports true when the last pass is done;
	// bound so a shader that never completes cannot spin forever.
	const int kMaxSteps = 4096;
	int steps = 0;
	while (steps < kMaxSteps && !s_shaderManager->CompileNextShader())
	{
		steps++;
	}

	if (steps >= kMaxSteps)
	{
		Printf(TEXTCOLOR_RED "hwrender: shader compilation did not converge after %d steps\n", kMaxSteps);
		return false;
	}

	// [rc4l] Fail with a name rather than letting a null spin the frame later.
	BackendPrereqs prereqs;
	prereqs.frameBuffer     = zx_screen != nullptr;
	prereqs.lightBuffer     = zx_screen != nullptr && zx_screen->mLights != nullptr;
	prereqs.boneBuffer      = zx_screen != nullptr && zx_screen->mBones != nullptr;
	prereqs.viewpointBuffer = zx_screen != nullptr && zx_screen->mViewpoints != nullptr;
	prereqs.vertexData      = zx_screen != nullptr && zx_screen->mVertexData != nullptr;
	prereqs.renderer        = OpenGLRenderer::GLRenderer != nullptr;
	prereqs.samplerManager  = OpenGLRenderer::GLRenderer != nullptr && OpenGLRenderer::GLRenderer->mSamplerManager != nullptr;
	prereqs.shaderManager   = OpenGLRenderer::GLRenderer != nullptr && OpenGLRenderer::GLRenderer->mShaderManager != nullptr;

	const char *missing = ComputeMissingPrereq(prereqs);
	if (missing != nullptr)
	{
		Printf(TEXTCOLOR_RED "hwrender: %s is null; the ported backend would spin on it\n", missing);
		return false;
	}

	// [rc4l] The compile loop counts iterations, not outcomes -- a failed link was being recorded as
	// success. Verify the programs actually linked and say why when they did not.
	{
		int checked = 0, failed = 0;
		for (int variant = 0; variant < 2; variant++)
		for (int eff = 0; eff < 16 && checked < 64; eff++)
		{
			OpenGLRenderer::FShader *sh = s_shaderManager->Get(eff, variant != 0, NORMAL_PASS);
			if (sh == nullptr) continue;
			const unsigned int handle = sh->GetHandle();
			checked++;
			GLint linked = GL_FALSE;
			glGetProgramiv(handle, GL_LINK_STATUS, &linked);
			if (linked == GL_TRUE) continue;
			failed++;
			if (failed <= 2)
			{
				GLint len = 0;
				glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &len);
				if (len > 1)
				{
					TArray<char> log(len + 1, true);
					glGetProgramInfoLog(handle, len, nullptr, log.Data());
					Printf(TEXTCOLOR_RED "hwrender: shader %d (alphatest=%d) link failed: %s\n", eff, variant, log.Data());
				}
				else
				{
					Printf(TEXTCOLOR_RED "hwrender: shader %d (alphatest=%d) link failed, no log, handle=%u\n", eff, variant, handle);
				}
			}
		}
		Printf("hwrender: %d/%d ported programs linked\n", checked - failed, checked);
	}

	Printf("hwrender: ported shader manager compiled in %d steps\n", steps);
	return true;
}

namespace
{
IRenderBackend *s_backend = nullptr;
Renderer2D *s_renderer2d = nullptr;
TextureHandle s_whiteTexture = TextureHandle::None;
bool s_frameInitFailed = false;

// [rc4l] 2D draws arrive throughout the frame but the core path owns presentation, so they are
// queued and replayed at Update() time rather than issued immediately.
struct Queued2D
{
	unsigned int texture;
	float x, y, w, h;
	float u0, v0, u1, v1;
	unsigned char r, g, b, a;
};
std::vector<Queued2D> s_queue2D;

// [rc4l] UZDoom's own 2D drawer, which owns render styles, translations and colour overlays -- all
// the DrawParms handling the bespoke queue below drops on the floor.
F2DDrawer s_drawer2D;

// [rc4l] Scene geometry for the current frame, in Doom coordinates; transformed by the scene MVP.
struct QueuedQuad
{
	unsigned int texture;
	std::vector<SceneVertex> verts;
	unsigned char r, g, b, a;
	bool translucent;
};
std::vector<QueuedQuad> s_queueScene;

Mat4 s_sceneProjection = ComputeIdentity();
Mat4 s_sceneView = ComputeIdentity();
bool s_haveSceneCamera = false;

// [rc4l] Lighting the legacy path applied via glColor4f; scene draws inherit it.
unsigned char s_surfR = 255, s_surfG = 255, s_surfB = 255, s_surfA = 255;

// [rc4l] Fog for the scene pass, as gl_SetFog computed it.
float s_fogR = 0.0f, s_fogG = 0.0f, s_fogB = 0.0f, s_fogDensity = 0.0f;
bool s_fogEnabled = false;

unsigned char ToByte(float v)
{
	const float scaled = v * 255.0f;
	if (scaled <= 0.0f) return 0;
	if (scaled >= 255.0f) return 255;
	return (unsigned char)(scaled + 0.5f);
}

// [rc4l] Resolves an engine FTexture to a live GL handle through the existing legacy texture path.
// 2D draws use the patch variant with the caller's translation -- the legacy 2D path bound via
// BindPatch, and paletted textures (fonts) draw rainbow garbage without their translation. The
// optional out-params receive the patch's valid UV window (see GetPatchGLHandle).
unsigned int GLHandleFor(FTexture *img, int translation = 0, bool patch = false,
	float *u1 = nullptr, float *v1 = nullptr, float *u2 = nullptr, float *v2 = nullptr)
{
	if (img == nullptr) return 0;
	FMaterial *mat = FMaterial::ValidateTexture(img);
	if (mat == nullptr) return 0;
	return patch ? mat->GetPatchGLHandle(translation, u1, v1, u2, v2)
	             : mat->GetBaseGLHandle(translation);
}

// [rc4l] Brings up the seam's backend + 2D path on first frame; false if it cannot be used.
bool EnsureFrameResources()
{
	if (s_frameInitFailed) return false;
	if (s_renderer2d != nullptr) return true;

	s_backend = CreateGLBackend();
	if (s_backend == nullptr || !s_backend->Init())
	{
		s_frameInitFailed = true;
		return false;
	}

	s_renderer2d = new Renderer2D();
	if (!s_renderer2d->Init(s_backend))
	{
		Printf(TEXTCOLOR_RED "hwrender: 2D shader failed to compile\n");
		s_frameInitFailed = true;
		return false;
	}

	// [rc4l] Flat colour source, so untextured fills go through the same textured path.
	const unsigned char white[4] = { 255, 255, 255, 255 };
	s_whiteTexture = s_backend->CreateTexture(1, 1, TextureFormat::RGBA8, white);
	return true;
}
}

void Queue2DTexture(FTexture *img, float x, float y, float w, float h, unsigned int rgba,
	int translation)
{
	// [rc4l] Sample the patch's valid UV window, not 0..1 -- patches carry pow2 padding and a 1px
	// expand border, and sampling the full texture draws them inside the quad (glyph gaps).
	float u1 = 0.0f, v1 = 0.0f, u2 = 1.0f, v2 = 1.0f;
	const unsigned int handle = GLHandleFor(img, translation, true, &u1, &v1, &u2, &v2);
	if (handle == 0) return;
	Queued2D q;
	q.texture = handle;
	q.x = x; q.y = y; q.w = w; q.h = h;
	q.u0 = u1; q.v0 = v1; q.u1 = u2; q.v1 = v2;
	q.r = (rgba >> 16) & 0xff;
	q.g = (rgba >> 8) & 0xff;
	q.b = rgba & 0xff;
	q.a = (rgba >> 24) & 0xff;
	s_queue2D.push_back(q);
}

void Queue2DTextureUV(FTexture *img, float x, float y, float w, float h,
	float u0, float v0, float u1, float v1, unsigned int rgba, int translation)
{
	const unsigned int handle = GLHandleFor(img, translation, true);
	if (handle == 0) return;
	Queued2D q;
	q.texture = handle;
	q.x = x; q.y = y; q.w = w; q.h = h;
	q.u0 = u0; q.v0 = v0; q.u1 = u1; q.v1 = v1;
	q.r = (rgba >> 16) & 0xff;
	q.g = (rgba >> 8) & 0xff;
	q.b = rgba & 0xff;
	q.a = (rgba >> 24) & 0xff;
	s_queue2D.push_back(q);
}

void Queue2DTextureLit(FTexture *img, float x, float y, float w, float h,
	float u0, float v0, float u1, float v1)
{
	const unsigned int rgba = ((unsigned int)s_surfA << 24) | ((unsigned int)s_surfR << 16) |
		((unsigned int)s_surfG << 8) | (unsigned int)s_surfB;
	Queue2DTextureUV(img, x, y, w, h, u0, v0, u1, v1, rgba);
}

void SetSceneCamera(float roll, float pitch, float yaw, float camX, float camY, float camZ,
	bool mirror, bool planeMirror, float fov, float ratio, float fovRatio)
{
	s_sceneProjection = ComputeSceneProjection(fov, ratio, fovRatio);
	s_sceneView = ComputeSceneView(roll, pitch, yaw, camX, camY, camZ, mirror, planeMirror);
	s_haveSceneCamera = true;
}

void QueueSceneQuad(FTexture *tex, const SceneVertex corners[4], unsigned int rgba)
{
	const unsigned int handle = GLHandleFor(tex);
	if (handle == 0) return;
	QueuedQuad q;
	q.texture = handle;
	q.verts.assign(corners, corners + 4);
	q.translucent = false;
	// [rc4l] rgba is the caller's tint slot; scene lighting comes from the captured glColor4f value.
	(void)rgba;
	q.r = s_surfR; q.g = s_surfG; q.b = s_surfB; q.a = s_surfA;
	s_queueScene.push_back(q);
}

void SetFogParams(float r, float g, float b, float density, bool enabled)
{
	s_fogR = r; s_fogG = g; s_fogB = b;
	// [rc4l] Same premultiply the legacy path applies before handing density to the shader.
	const float LOG2E = 1.442692f;
	s_fogDensity = density * (-LOG2E / 64000.f);
	s_fogEnabled = enabled && density != 0.0f;
}

void SetSurfaceColor(float r, float g, float b, float a)
{
	s_surfR = ToByte(r); s_surfG = ToByte(g); s_surfB = ToByte(b); s_surfA = ToByte(a);
}

void QueueSceneFan(FTexture *tex, const SceneVertex *verts, int count, unsigned int rgba, bool translucent,
	int translation, bool patchTex)
{
	if (count < 3) return;
	const unsigned int handle = GLHandleFor(tex, translation, patchTex);
	if (handle == 0) return;
	QueuedQuad q;
	q.texture = handle;
	q.verts.assign(verts, verts + count);
	q.translucent = translucent;
	(void)rgba;
	q.r = s_surfR; q.g = s_surfG; q.b = s_surfB; q.a = s_surfA;
	s_queueScene.push_back(q);
}

void Add2DTexture(FTexture *img, DCanvas::DrawParms &parms)
{
	if (img == nullptr) return;
	s_drawer2D.AddTexture(img, parms);
}

void RenderCoreFrame(int width, int height)
{
	if (!EnsureFrameResources())
	{
		s_queue2D.clear();
		s_queueScene.clear();
		// [rc4l] Nothing usable came up; leave a clean black frame rather than whatever was last drawn.
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		return;
	}

	s_backend->BeginFrame(width, height);
	glClearColor(0.10f, 0.10f, 0.14f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// [rc4l] Scene first, depth-tested, so the 2D layer composites on top of it.
	if (s_haveSceneCamera && !s_queueScene.empty())
	{
		const Mat4 mvp = ComputeMultiply(s_sceneProjection, s_sceneView);
		s_renderer2d->SetFog(s_fogR, s_fogG, s_fogB, s_fogDensity, s_fogEnabled);
		s_renderer2d->BeginScene(mvp);
		// [rc4l] Two passes, not one insertion-ordered list. Translucent draws do not write depth, so
		// any opaque surface queued after a sprite would still overwrite the depth the sprite tested
		// against and erase part of it -- which is what "enemies cut in half" looks like.
		for (int pass = 0; pass < 2; pass++)
		{
			const bool wantTranslucent = (pass == 1);
			for (size_t i = 0; i < s_queueScene.size(); i++)
			{
				const QueuedQuad &q = s_queueScene[i];
				if (q.translucent != wantTranslucent) continue;
				s_renderer2d->DrawSceneFan((TextureHandle)q.texture, q.verts.data(), (int)q.verts.size(),
					q.r, q.g, q.b, q.a, q.translucent);
			}
		}
	}
	s_queueScene.clear();

	// [rc4l] The queue. F2DDrawer is wired and raises only one GL error per frame, which is far too
	// few to explain 54 dropped draws -- see PLAN.md; the cause is elsewhere.
	s_renderer2d->Begin(width, height);
	for (size_t i = 0; i < s_queue2D.size(); i++)
	{
		const Queued2D &q = s_queue2D[i];
		s_renderer2d->DrawTexturedQuad((TextureHandle)q.texture,
			q.x, q.y, q.w, q.h,
			q.u0, q.v0, q.u1, q.v1,
			q.r, q.g, q.b, q.a,
			BlendMode::Translucent);
	}
	s_queue2D.clear();
	s_drawer2D.Clear();

	s_backend->EndFrame();
}

void ShutdownPortedShaders()
{
	if (s_shaderManager == nullptr) return;
	delete s_shaderManager;
	s_shaderManager = nullptr;
}

} // namespace hwrender

#else // NO_GL

namespace hwrender
{
void SetCoreProfile(bool) {}
bool IsCoreProfile() { return false; }
void RenderCoreFrame(int, int) {}
void Queue2DTexture(FTexture *, float, float, float, float, unsigned int) {}
void Add2DTexture(FTexture *, DCanvas::DrawParms &) {}
void Queue2DTextureUV(FTexture *, float, float, float, float, float, float, float, float, unsigned int) {}
void Queue2DTextureLit(FTexture *, float, float, float, float, float, float, float, float) {}
void SetSceneCamera(float, float, float, float, float, float, bool, bool, float, float, float) {}
void QueueSceneQuad(FTexture *, const SceneVertex *, unsigned int) {}
void QueueSceneFan(FTexture *, const SceneVertex *, int, unsigned int, bool, int, bool) {}
void SetSurfaceColor(float, float, float, float) {}
void SetFogParams(float, float, float, float, bool) {}
bool InitPortedShaders() { return false; }
void ShutdownPortedShaders() {}
} // namespace hwrender

#endif // NO_GL
