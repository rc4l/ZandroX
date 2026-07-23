// [rc4l] Covers every branch of ComputeClampMode.
#include <gtest/gtest.h>

#include "clampmode_compute.h"

TEST(ClampMode, SoftwareCanvasIsNeverFiltered)
{
	// [rc4l] Software canvas wins over every other trait.
	EXPECT_EQ(CLAMP_NOFILTER, ComputeClampMode(CLAMP_XY, true, true, true, true));
}

TEST(ClampMode, HardwareCanvasUsesTheCameraTextureSampler)
{
	EXPECT_EQ(CLAMP_CAMTEX, ComputeClampMode(CLAMP_XY, false, true, true, true));
}

TEST(ClampMode, WarpedOrUserShaderDropsClampingWithinTheXYRange)
{
	EXPECT_EQ(CLAMP_NONE, ComputeClampMode(CLAMP_XY, false, false, true, false));
	EXPECT_EQ(CLAMP_NONE, ComputeClampMode(CLAMP_X, false, false, false, true));
	EXPECT_EQ(CLAMP_NONE, ComputeClampMode(CLAMP_NONE, false, false, true, false));
}

TEST(ClampMode, WarpedShaderKeepsModesAboveTheXYRange)
{
	// [rc4l] CLAMP_XY_NOMIP is past the cutoff, so the warp branch must leave it alone.
	EXPECT_EQ(CLAMP_XY_NOMIP, ComputeClampMode(CLAMP_XY_NOMIP, false, false, true, true));
}

TEST(ClampMode, PlainTextureKeepsTheRequestedMode)
{
	EXPECT_EQ(CLAMP_XY, ComputeClampMode(CLAMP_XY, false, false, false, false));
	EXPECT_EQ(CLAMP_NOFILTER_XY, ComputeClampMode(CLAMP_NOFILTER_XY, false, false, false, false));
}
