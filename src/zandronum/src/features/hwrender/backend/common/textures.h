// [rc4l] Shim: UZDoom's textures.h sits at the include root; ours is under textures/.
#pragma once

#include "textures/textures.h"
#include "zx_texconst.h"
#include "zx_usershader.h"

// [rc4l] UZDoom's modern texture/type vocabulary mapped onto ours, the same adapter approach P1 used
// for FMaterial. FGameTexture is their FTexture+metadata split; ours is the single FTexture that
// already grew the accessors they expect (isHardwareCanvas, GetTexture, GetClampMode, ...).
typedef FTexture FGameTexture;

// [rc4l] They wrap translation ids in a struct; ours are plain ints.
typedef int FTranslationID;

// [rc4l] Their texture-mode enum; the values are the TM_* constants in zx_texconst.h.
typedef int ETexMode;
