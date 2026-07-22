// [rc4l] Minimal ZXFrameBuffer implementation. Upstream's lives in v_video.cpp alongside their 2D
// drawer, texture manager and wipe machinery, none of which we have ported; these bodies cover the
// handful of non-inline members the shader and buffer paths need to link.
#include "zx_video.h"
#include "hw_shadowmap.h"

// [rc4l] ZXFrameBuffer holds an IShadowMap by value, so its destructor must exist; shadow maps are
// part of the postprocess chain we have not ported.
IShadowMap::~IShadowMap()
{
}

ZXFrameBuffer::ZXFrameBuffer (int width, int height)
{
	SetSize(width, height);
}

ZXFrameBuffer::~ZXFrameBuffer()
{
}

void ZXFrameBuffer::Update ()
{
}

void ZXFrameBuffer::SetVSync (bool)
{
}

// [rc4l] Material creation stays on our own FMaterial path; see the adapter in gl/textures/gl_material.cpp.
FMaterial* ZXFrameBuffer::CreateMaterial(FGameTexture*, int)
{
	return nullptr;
}

// [rc4l] Screen wipes are still driven by our legacy renderer.
FTexture *ZXFrameBuffer::WipeStartScreen()
{
	return nullptr;
}

FTexture *ZXFrameBuffer::WipeEndScreen()
{
	return nullptr;
}

void ZXFrameBuffer::SetViewportRects(IntRect *)
{
}

void ZXFrameBuffer::SetSize(int width, int height)
{
	Width = width;
	Height = height;
}

// [rc4l] Maps a screen coordinate into window space. Their framebuffer can letterbox into a larger
// window; ours renders at the window's own size, so these are identity.
int ZXFrameBuffer::ScreenToWindowX(int x)
{
	return x;
}

int ZXFrameBuffer::ScreenToWindowY(int y)
{
	return y;
}
