// [rc4l] UZDoom declares these in its textures.h; we have no GLDEFS user-shader system, so the list stays empty and the shader manager's user-shader pass compiles nothing.
#pragma once

#include "zstring.h"
#include "tarray.h"
#include "zx_texconst.h"

struct UserShaderDesc
{
	FString shader;
	MaterialShaderIndex shaderType;
	FString defines;
	bool disablealphatest = false;
	uint8_t shaderFlags = 0;
};

extern TArray<UserShaderDesc> usershaders;
