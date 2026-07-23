// [rc4l] Pure clamp-mode selection extracted from UZDoom's FGameTexture::GetClampMode so it can be unit-tested.
#pragma once

#include "features/hwrender/backend/hwrenderer/data/zx_texconst.h"

// [rc4l] Narrows a requested clamp mode to the one a texture's nature actually permits.
int ComputeClampMode(int clampmode, bool isSoftwareCanvas, bool isHardwareCanvas, bool isWarped, bool isUserShader);
