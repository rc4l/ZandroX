/*
** hw_draw2d.cpp
**
** 2d drawer Renderer interface
**
**---------------------------------------------------------------------------
**
** Copyright 2018-2019 Christoph Oelckers
** Copyright 2017-2025 GZDoom Maintainers and Contributors
** Copyright 2025-2026 UZDoom Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**---------------------------------------------------------------------------
**
** Code written prior to 2026 is also licensed under:
**
** SPDX-License-Identifier: BSD-3-Clause
**
**---------------------------------------------------------------------------
**
*/

#include "zx_video.h"
#include "cmdlib.h"
// [rc4l] GL entry points for the temporary draw probe below.
#include "gl_system.h"
#include "buffers.h"
#include "flatvertices.h"
#include "hw_viewpointbuffer.h"
#include "hw_clock.h"
#include "hw_cvars.h"
#include "hw_renderstate.h"
#include "r_videoscale.h"
#include "v_2ddrawer.h" // [rc4l] F2DDrawer and its command types.
#include "zx_texconst.h"
#include "v_draw.h"

//===========================================================================
//
// Draws the 2D stuff. This is the version for OpenGL 3 and later.
//
//===========================================================================

// [rc4l] Defined by our gl_framebuffer.cpp; take theirs as a reference to avoid a duplicate.
EXTERN_CVAR(Bool, gl_aalines)
CVAR(Bool, hw_2dmip, true, CVAR_ARCHIVE)

// [rc4l] Defined below; declared here because the 2-argument form calls it.

void Draw2D(F2DDrawer* drawer, FRenderState& state, int x, int y, int width, int height);

void Draw2D(F2DDrawer* drawer, FRenderState& state)
{
	const auto& mScreenViewport = zx_screen->mScreenViewport;
	Draw2D(drawer, state, mScreenViewport.left, mScreenViewport.top, mScreenViewport.width, mScreenViewport.height);
}

void Draw2D(F2DDrawer* drawer, FRenderState& state, int x, int y, int width, int height)
{
	twoD.Clock();

	state.SetViewport(x, y, width, height);
	zx_screen->mViewpoints->Set2D(state, drawer->GetWidth(), drawer->GetHeight());

	state.EnableStencil(false);
	state.SetStencil(0, SOP_Keep, SF_AllOn);
	state.Clear(CT_Stencil);
	state.EnableDepthTest(false);
	state.EnableMultisampling(false);
	state.EnableLineSmooth(gl_aalines);

	bool cache_hw_2dmip = hw_2dmip && (!sysCallbacks.DisableAnisotropicFiltering || !sysCallbacks.DisableAnisotropicFiltering()); // cache cvar lookup so it's not done in a loop

	auto &vertices = drawer->mVertices;
	auto &indices = drawer->mIndices;
	auto &commands = drawer->mData;

	if (commands.Size() == 0)
	{
		twoD.Unclock();
		return;
	}

	if (drawer->mIsFirstPass)
	{
		for (auto &v : vertices)
		{
			// Change from BGRA to RGBA
			std::swap(v.color0.r, v.color0.b);
		}
	}
	F2DVertexBuffer vb;
	vb.UploadData(&vertices[0], vertices.Size(), &indices[0], indices.Size());
	state.SetVertexBuffer(&vb);
	state.EnableFog(false);

	for(auto &cmd : commands)
	{
		if (cmd.isSpecial != SpecialDrawCommand::NotSpecial)
		{
			if (cmd.isSpecial == SpecialDrawCommand::EnableStencil)
			{
				state.EnableStencil(cmd.stencilOn);
			}
			else if (cmd.isSpecial == SpecialDrawCommand::SetStencil)
			{
				state.SetStencil(cmd.stencilOffs, cmd.stencilOp, cmd.stencilFlags);
			}
			else if (cmd.isSpecial == SpecialDrawCommand::ClearStencil)
			{
				state.Clear(CT_Stencil);
			}
			continue;
		}

		state.SetRenderStyle(cmd.mRenderStyle);
		state.EnableBrightmap(!(cmd.mRenderStyle.Flags & STYLEF_ColorIsFixed));
		state.EnableFog(2);	// Special 2D mode 'fog'.
		state.SetScreenFade(cmd.mScreenFade);

		state.SetTextureMode(cmd.mDrawMode);

		int sciX, sciY, sciW, sciH;
		if (cmd.mFlags & F2DDrawer::DTF_Scissor)
		{
			// scissor test doesn't use the current viewport for the coordinates, so use real screen coordinates
			// Note that the origin here is the lower left corner!
			sciX = zx_screen->ScreenToWindowX(cmd.mScissor[0]);
			sciY = zx_screen->ScreenToWindowY(cmd.mScissor[3]);
			sciW = zx_screen->ScreenToWindowX(cmd.mScissor[2]) - sciX;
			sciH = zx_screen->ScreenToWindowY(cmd.mScissor[1]) - sciY;
			// If coordinates turn out negative, clip to sceen here to avoid undefined behavior.
			if (sciX < 0) sciW += sciX, sciX = 0;
			if (sciY < 0) sciH += sciY, sciY = 0;
		}
		else
		{
			sciX = sciY = sciW = sciH = -1;
		}
		state.SetScissor(sciX, sciY, sciW, sciH);

		if (cmd.mSpecialColormap[0].a != 0)
		{
			state.SetTextureMode(TM_FIXEDCOLORMAP);
			state.SetObjectColor(cmd.mSpecialColormap[0]);
			state.SetAddColor(cmd.mSpecialColormap[1]);
		}
		state.SetFog(cmd.mColor1, 0);
		state.SetColor(1, 1, 1, 1, cmd.mDesaturate);
		if (cmd.mFlags & F2DDrawer::DTF_Indexed) state.SetSoftLightLevel(cmd.mLightLevel);
		state.SetLightParms(0, 0);

		state.AlphaFunc(Alpha_Greater, 0.f);

		if (cmd.useTransform)
		{
			FLOATTYPE m[16] = {
				0.0, 0.0, 0.0, 0.0,
				0.0, 0.0, 0.0, 0.0,
				0.0, 0.0, 1.0, 0.0,
				0.0, 0.0, 0.0, 1.0
			};
			for (size_t i = 0; i < 2; i++)
			{
				for (size_t j = 0; j < 2; j++)
				{
					m[4 * j + i] = (FLOATTYPE) cmd.transform.Cells[i][j];
				}
			}
			for (size_t i = 0; i < 2; i++)
			{
				m[4 * 3 + i] = (FLOATTYPE) cmd.transform.Cells[i][2];
			}
			state.mModelMatrix.loadMatrix(m);
			state.EnableModelMatrix(true);
		}

		if (cmd.mTexture != nullptr && cmd.mTexture->isValid())
		{
			auto flags = cmd.mTexture->GetUseType() >= ETextureType::Special? UF_None : cmd.mTexture->GetUseType() == ETextureType::FontChar? UF_Font : UF_Texture;

			auto scaleflags = cmd.mFlags & F2DDrawer::DTF_Indexed ? CTF_Indexed : 0;
			state.SetMaterial(cmd.mTexture, flags, scaleflags, cmd.mFlags & F2DDrawer::DTF_Wrap ? CLAMP_NONE : (cache_hw_2dmip ? CLAMP_XY : CLAMP_XY_NOMIP), cmd.mTranslationId, -1);
			state.EnableTexture(true);

			// Canvas textures are stored upside down
			if (cmd.mTexture->isHardwareCanvas())
			{
				state.mTextureMatrix.loadIdentity();
				state.mTextureMatrix.scale(1.f, -1.f, 1.f);
				state.mTextureMatrix.translate(0.f, 1.f, 0.0f);
				state.EnableTextureMatrix(true);
			}
			if (cmd.mFlags & F2DDrawer::DTF_Burn)
			{
				state.SetEffect(EFF_BURN);
			}
		}
		else
		{
			state.EnableTexture(false);
		}

		if (cmd.shape2DBufInfo != nullptr)
		{
			state.SetVertexBuffer(&cmd.shape2DBufInfo->buffers[cmd.shape2DBufIndex]);
			state.DrawIndexed(DT_Triangles, 0, cmd.shape2DIndexCount);
			state.SetVertexBuffer(&vb);
			if (cmd.shape2DCommandCounter == cmd.shape2DBufInfo->lastCommand)
			{
				cmd.shape2DBufInfo->lastCommand = -1;
				if (cmd.shape2DBufInfo->bufIndex > 0)
				{
					cmd.shape2DBufInfo->needsVertexUpload = true;
					cmd.shape2DBufInfo->buffers.Clear();
					cmd.shape2DBufInfo->bufIndex = -1;
				}
			}
			cmd.shape2DBufInfo->uploadedOnce = false;
		}
		else
		{
			switch (cmd.mType)
			{
			default:
			case F2DDrawer::DrawTypeTriangles:
				// [rc4l] Temporary probe (first command of the first frames): pin down where the
				// one-per-frame GL error comes from and what program/viewport the draw actually
				// runs with -- invisible-yet-errorless draws point at zeroed transforms.
				{
					static int drawProbes = 0;
					const bool p = drawProbes < 3;
					int preErrs = 0, postErrs = 0;
					if (p) { while (glGetError() != GL_NO_ERROR) preErrs++; }
					state.DrawIndexed(DT_Triangles, cmd.mIndexIndex, cmd.mIndexCount);
					if (p)
					{
						drawProbes++;
						while (glGetError() != GL_NO_ERROR) postErrs++;
						GLint prog = -1, vp[4] = {-1,-1,-1,-1}, vao = -1, elem = -1;
						glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
						glGetIntegerv(GL_VIEWPORT, vp);
						glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
						glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &elem);
						Printf("hwrender draw probe: preErrs=%d postErrs=%d prog=%d vp=(%d,%d,%d,%d) vao=%d elem=%d idx=%d cnt=%d\n",
							preErrs, postErrs, prog, vp[0], vp[1], vp[2], vp[3], vao, elem,
							cmd.mIndexIndex, cmd.mIndexCount);
					}
				}
				break;

			case F2DDrawer::DrawTypeLines:
				state.Draw(DT_Lines, cmd.mVertIndex, cmd.mVertCount);
				break;

			case F2DDrawer::DrawTypePoints:
				state.Draw(DT_Points, cmd.mVertIndex, cmd.mVertCount);
				break;

			}
		}
		state.SetObjectColor(0xffffffff);
		state.SetObjectColor2(0);
		state.SetAddColor(0);
		state.EnableTextureMatrix(false);
		state.EnableModelMatrix(false);
		state.SetEffect(EFF_NONE);

	}
	state.SetScissor(-1, -1, -1, -1);

	state.SetRenderStyle(STYLE_Translucent);
	state.SetVertexBuffer(zx_screen->mVertexData);
	state.EnableStencil(false);
	state.SetStencil(0, SOP_Keep, SF_AllOn);
	state.EnableTexture(true);
	state.EnableBrightmap(true);
	state.SetTextureMode(TM_NORMAL);
	state.EnableFog(false);
	state.SetScreenFade(1);
	state.SetSoftLightLevel(255);
	state.ResetColor();
	drawer->mIsFirstPass = false;
	twoD.Unclock();
}
