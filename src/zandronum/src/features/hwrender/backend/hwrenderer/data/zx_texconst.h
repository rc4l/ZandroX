// [rc4l] Texture clamp modes and built-in shader indices from UZDoom, split out so textures.h and the computation units can use them without including the heavy hw_material.h bridge.
#pragma once

// [rc4l] Texture clamp/sampler modes from UZDoom's textures.h; the backend indexes its sampler array by these.
enum
{
	CLAMP_NONE = 0,
	CLAMP_X,
	CLAMP_Y,
	CLAMP_XY,
	CLAMP_XY_NOMIP,
	CLAMP_NOFILTER,
	CLAMP_NOFILTER_X,
	CLAMP_NOFILTER_Y,
	CLAMP_NOFILTER_XY,
	CLAMP_CAMTEX,
	NUMSAMPLERS
};

// [rc4l] UZDoom's built-in shader indices; the backend compares effect ids against these.
enum MaterialShaderIndex
{
	SHADER_Default,
	SHADER_Warp1,
	SHADER_Warp2,
	SHADER_Specular,
	SHADER_PBR,
	SHADER_Paletted,
	SHADER_NoTexture,
	SHADER_BasicFuzz,
	SHADER_SmoothFuzz,
	SHADER_SwirlyFuzz,
	SHADER_TranslucentFuzz,
	SHADER_JaggedFuzz,
	SHADER_NoiseFuzz,
	SHADER_SmoothNoiseFuzz,
	SHADER_SoftwareFuzz,
	FIRST_USER_SHADER
};

// [rc4l] UZDoom's texture modes; ours (gl_interface.h) use the same TM_OPAQUE name with a different value, hence the isolation above.
enum
{
	TM_NORMAL = 0,
	TM_STENCIL,
	TM_OPAQUE,
	TM_INVERSE,
	TM_ALPHATEXTURE,
	TM_CLAMPY,
	TM_OPAQUEINVERSE,
	TM_FOGLAYER,
	TM_FIXEDCOLORMAP,
};

// [rc4l] Name their 2D drawer uses for the opaque+inverse mode.
static const int TM_INVERTOPAQUE = TM_OPAQUEINVERSE;

// [rc4l] UZDoom's texture-type and upscale-flag enums, needed by their 2D emitter. Our engine uses
// the older FTexture::TEX_* use-types; these exist so their ported code compiles unchanged.
enum class ETextureType : unsigned char
{
	Any, Wall, Flat, Sprite, WallPatch, Build, SkinSprite, Decal, MiscPatch, FontChar,
	Override, Autopage, SkinGraphic, Null, FirstDefined, Special, SWCanvas,
};

enum EUpscaleFlags : int
{
	UF_None = 0,
	UF_Texture = 1,
	UF_Sprite = 2,
	UF_Font = 4,
	UF_Skin = 8,
};
