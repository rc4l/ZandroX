// [rc4l] See clampmode_compute.h.
#include "clampmode_compute.h"

int ComputeClampMode(int clampmode, bool isSoftwareCanvas, bool isHardwareCanvas, bool isWarped, bool isUserShader)
{
	// [rc4l] Software canvases are blitted verbatim, so they must never be filtered.
	if (isSoftwareCanvas) return CLAMP_NOFILTER;
	// [rc4l] Hardware canvases render into a camera texture, which has its own sampler.
	if (isHardwareCanvas) return CLAMP_CAMTEX;
	// [rc4l] Warp and user shaders sample outside the base rectangle, so clamping would cut the effect off.
	if ((isWarped || isUserShader) && clampmode <= CLAMP_XY) return CLAMP_NONE;
	return clampmode;
}
