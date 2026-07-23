// [rc4l] Shim for UZDoom's v_draw.h. Their DrawParms sits at namespace scope; ours is nested inside
// DCanvas, so it is aliased out here to let their 2D drawer compile unmodified.
#pragma once

#include "v_video.h"

typedef DCanvas::DrawParms DrawParms;
