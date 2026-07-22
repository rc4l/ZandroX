// [rc4l] Texture-side bridge for the ported backend (see issue #4).
#include "doomtype.h"
#include "textures/textures.h"
#include "zx_texbuffer.h"
#include "textures/bitmap.h"
#include "gl/textures/gl_hwtexture.h"

// [rc4l] The P1 assumption -- that FMaterial::GetLayer's adopted GL id always short-circuits the
// uploader's create-branch -- does not hold for the 2D path: Bind() misses and the create-branch
// runs, so this has to produce real pixels. Our FTexture already renders true-colour through
// CopyTrueColorPixels, so this bridges to it rather than porting their FGameTexture system
// (issue #4). FTextureBuffer owns and frees the buffer.
FTextureBuffer FTexture::CreateTexBuffer(int translation, int flags)
{
	FTextureBuffer result;

	const int w = GetWidth();
	const int h = GetHeight();
	if (w <= 0 || h <= 0) return result;

	// [rc4l] Pad to the same power-of-two dimensions the legacy uploader uses. The engine's texture
	// coordinates are computed against the padded size -- a 45-wide sprite in a 64-wide texture is
	// addressed over u in [0, 0.703] -- so uploading at the unpadded size makes those coordinates
	// reach only part of the image and slices the sprite vertically.
	const int rw = FHardwareTexture::GetTexDimension(w);
	const int rh = FHardwareTexture::GetTexDimension(h);

	unsigned char *buffer = new unsigned char[rw * rh * 4];
	memset(buffer, 0, rw * rh * 4);

	FBitmap bmp(buffer, rw * 4, rw, rh);
	CopyTrueColorPixels(&bmp, 0, 0);

	result.mBuffer = buffer;
	result.mWidth = rw;
	result.mHeight = rh;
	return result;
}

// [rc4l] The ported uploader references these from its create-branch, which our adopt-path never
// reaches; define them minimally so the backend links during coexistence (see issue #4).
#include "hw_ihwtexture.h"
// [rc4l] GL entry points must be declared before gl_renderer.h pulls in the render buffers.
#include "gl_system.h"
#include "gl_renderer.h"

// [rc4l] Forward-declared rather than including gl_renderer.h, which cascades into GL-typed headers.
namespace OpenGLRenderer
{
	// [rc4l] Defined here rather than by their gl_renderer.cpp, which we do not compile. It is
	// populated by hwrender::InitPortedShaders once a core context exists -- the texture uploader
	// dereferences GLRenderer->mSamplerManager on every bind, so a null here spins the frame.
	FGLRenderer *GLRenderer = nullptr;

	// [rc4l] Minimal lifecycle. Their gl_renderer.cpp builds the whole renderer -- render buffers,
	// postprocess, present shaders -- none of which we drive; we only need the object to exist so
	// its mSamplerManager and mShaderManager can be reached.
	FGLRenderer::FGLRenderer(OpenGLFrameBuffer *fb) : framebuffer(fb)
	{
	}

	FGLRenderer::~FGLRenderer()
	{
	}
}

// [rc4l] Image rescaling used only when the ported uploader creates its own texture, which we bypass.
void IHardwareTexture::Resize(int swidth, int sheight, int width, int height, unsigned char *src_data, unsigned char *dst_data)
{
}

// [rc4l] GPU timing stat toggle from UZDoom's stats system, which we do not port.
bool gpuStatActive = false;

// [rc4l] Remaining GPU-stat globals from UZDoom's stats system, which we do not port.
#include "zstring.h"
bool keepGpuStatActive = false;
FString gpuStatOutput;

// [rc4l] Globals the ported backend declares but whose defining TUs are not compiled yet.
#include "zx_glcycle.h"
#include "hw_clock.h"
#include "zx_video.h"

bool glcycle_t::active = false;
glcycle_t drawcalls, twoD, Flush3D;
ZXFrameBuffer *zx_screen = nullptr;

// [rc4l] UZDoom's lump-to-string helper, on our FWadCollection.
#include "filesystem.h"

FString GetStringFromLump(int lump, bool zerotruncate)
{
	FMemLump data = Wads.ReadLump(lump);
	FString ret(static_cast<const char *>(data.GetMem()), Wads.LumpLength(lump));
	if (zerotruncate) ret.Truncate((long)strlen(ret.GetChars()));
	return ret;
}

// [rc4l] Empty until a GLDEFS user-shader system is ported; see zx_usershader.h.
#include "zx_usershader.h"

TArray<UserShaderDesc> usershaders;

// [rc4l] CVARs the ported backend reads but whose defining TUs (v_video.cpp, hw_cvars.cpp) are not ported.
#include "c_cvars.h"

CVAR(Bool, r_skipmats, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, gl_shadowmap_filter, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
