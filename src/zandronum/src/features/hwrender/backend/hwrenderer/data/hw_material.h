// [rc4l] Bridge header mapping the ported backend's texture/material/style expectations onto Zandronum's equivalents.
#pragma once

// [rc4l] Our FRenderStyle lives here; it carries no TM_* enum, so it is safe to pull into backend TUs.
#include "r_data/renderstyle.h"

// [rc4l] Safe to include in full: gl_material.h's chain stops at gl_hwtexture.h/gl_colormap.h and never reaches gl_interface.h, whose TM_* constants would collide with UZDoom's below.
#include "gl/textures/gl_material.h"

class FTexture;

#include "zx_materiallayer.h"
#include "zx_texbuffer.h"
#include "zx_texconst.h"



// [rc4l] UZDoom's texture flags, Or'ed into uTextureMode above its 3 lowermost bits; we have no equivalent.
enum texflags
{
	TEXF_Brightmap = 0x10000,
	TEXF_Detailmap = 0x20000,
	TEXF_Glowmap = 0x40000,
	TEXF_ClampY = 0x80000,
};


// [rc4l] UZDoom has DefaultRenderStyle() in its renderstyle.h; ours only exposes the LegacyRenderStyles table.
inline FRenderStyle DefaultRenderStyle()
{
	return LegacyRenderStyles[STYLE_Normal];
}

// [rc4l] UZDoom's FMaterial exposes GetLayerFlags()/GetDetailScale() for its layered texture system (brightmaps, detail maps, upscaling); ours has no equivalent, so these report "none".
struct ZXDetailScale
{
	float X, Y;
};

inline int ZX_MaterialLayerFlags(FMaterial *) { return 0; }

inline ZXDetailScale ZX_MaterialDetailScale(FMaterial *) { return ZXDetailScale{ 1.0f, 1.0f }; }
