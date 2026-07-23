// [rc4l] Implementation of the 2D/HUD draw path over the hwrender seam. Pure geometry/matrix work
// is delegated to computation/ (tested); this file is the thin backend glue. No direct GL calls.
#include "features/hwrender/renderer2d.h"
#include "features/hwrender/hwrender_init.h"
#include <vector>
#include "features/hwrender/shaders.h"
#include "features/hwrender/computation/matrix_compute.h"
#include "features/hwrender/computation/quad_compute.h"
#include "features/hwrender/computation/renderstate_compute.h"

namespace hwrender
{

namespace
{
// [rc4l] Attribute layout matching shaders.h: pos(vec3 f32), uv(vec2 f32), color(rgba8).
const VertexAttrib kLayout[3] = {
	{3, AttribType::Float32},
	{2, AttribType::Float32},
	{4, AttribType::UNorm8},
};
} // namespace

bool Renderer2D::Init(IRenderBackend *backend)
{
	mBackend = backend;
	mShader = backend->CreateShader(kBaseVertexShader, kBaseFragmentShader);
	if (mShader == ShaderHandle::None)
		return false;

	mMvpLoc = backend->GetUniformLocation(mShader, "uMVP");
	mTexLoc = backend->GetUniformLocation(mShader, "uTexture");
	mAlphaLoc = backend->GetUniformLocation(mShader, "uAlphaThreshold");
	mFogColorLoc = backend->GetUniformLocation(mShader, "uFogColor");
	mFogDensityLoc = backend->GetUniformLocation(mShader, "uFogDensity");

	// One quad's worth of dynamic vertex storage, refilled per draw.
	mVbo = backend->CreateBuffer(BufferKind::Vertex, nullptr, sizeof(Vertex2D) * 6, true);
	return true;
}

void Renderer2D::Shutdown()
{
	if (mBackend == nullptr)
		return;
	if (mVbo != BufferHandle::None) mBackend->DestroyBuffer(mVbo);
	if (mShader != ShaderHandle::None) mBackend->DestroyShader(mShader);
	mVbo = BufferHandle::None;
	mShader = ShaderHandle::None;
	mBackend = nullptr;
}

void Renderer2D::Begin(int screenW, int screenH)
{
	// [rc4l] The HUD and menus are screen-space; fog must not touch them.
	SetFog(0.0f, 0.0f, 0.0f, 0.0f, false);

	// [rc4l] Screen-space ortho: (0,0) top-left, (w,h) bottom-right.
	const Mat4 mvp = ComputeOrtho(0.0f, (float)screenW, (float)screenH, 0.0f, -1.0f, 1.0f);
	mBackend->UseShader(mShader);
	mBackend->SetUniformMat4(mMvpLoc, mvp);
	mBackend->SetUniform1i(mTexLoc, 0);
	mBackend->SetDepthTest(false, false);
	mBackend->SetCull(false);
	mBackend->SetViewport(0, 0, screenW, screenH);
}

void Renderer2D::SetFog(float r, float g, float b, float density, bool enabled)
{
	mBackend->UseShader(mShader);
	mBackend->SetUniform4f(mFogColorLoc, r, g, b, enabled ? 1.0f : 0.0f);
	mBackend->SetUniform1f(mFogDensityLoc, density);
}

void Renderer2D::BeginScene(const Mat4 &mvp)
{
	mBackend->UseShader(mShader);
	mBackend->SetUniformMat4(mMvpLoc, mvp);
	mBackend->SetUniform1i(mTexLoc, 0);
	// [rc4l] Scene geometry is depth-tested; the 2D pass that follows turns this back off.
	mBackend->SetDepthTest(true, true);
	mBackend->SetCull(false);
}

void Renderer2D::DrawSceneQuad(TextureHandle tex, const SceneVertex *corners,
                               unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	// [rc4l] The legacy path emits a triangle fan of 4 corners; two triangles are the core equivalent.
	const int order[6] = { 0, 1, 2, 0, 2, 3 };
	Vertex2D verts[6];
	for (int i = 0; i < 6; i++)
	{
		const SceneVertex &c = corners[order[i]];
		verts[i].x = c.x; verts[i].y = c.y; verts[i].z = c.z;
		verts[i].u = c.u; verts[i].v = c.v;
		verts[i].r = r; verts[i].g = g; verts[i].b = b; verts[i].a = a;
	}

	mBackend->SetBlendState(ComputeBlendState(BlendMode::Opaque));
	mBackend->BindTexture(0, tex);
	mBackend->UpdateBuffer(mVbo, verts, sizeof(verts));

	const int stride = ComputeVertexStride(kLayout, 3);
	int offsets[3];
	ComputeAttribOffsets(kLayout, 3, offsets);
	const AttribBinding bindings[3] = {
		{0, kLayout[0], offsets[0], false},
		{1, kLayout[1], offsets[1], false},
		{2, kLayout[2], offsets[2], true},
	};
	mBackend->SetVertexBuffer(mVbo, bindings, 3, stride);
	mBackend->Draw(Primitive::Triangles, 0, 6);
}

void Renderer2D::DrawSceneFan(TextureHandle tex, const SceneVertex *verts, int count,
                              unsigned char r, unsigned char g, unsigned char b, unsigned char a,
                              bool translucent)
{
	if (count < 3) return;

	// [rc4l] Expand the fan to independent triangles; the buffer grows on upload as needed.
	std::vector<Vertex2D> tris;
	tris.reserve((count - 2) * 3);
	for (int i = 1; i + 1 < count; i++)
	{
		const int idx[3] = { 0, i, i + 1 };
		for (int k = 0; k < 3; k++)
		{
			const SceneVertex &c = verts[idx[k]];
			Vertex2D v;
			v.x = c.x; v.y = c.y; v.z = c.z;
			v.u = c.u; v.v = c.v;
			v.r = r; v.g = g; v.b = b; v.a = a;
			tris.push_back(v);
		}
	}

	// [rc4l] Sprites blend against the geometry already in the depth buffer, so they test depth but
	// must not write it, or a sprite's transparent border would occlude what is behind it.
	// A -1.0/-128.0 depth bias here -- the offset the legacy decal pass uses -- was tried and did
	// not fix the truncated sprites, so it is not applied; see PLAN.md.
	mBackend->SetDepthTest(true, !translucent);
	mBackend->SetDepthBias(0.0f, 0.0f);
	mBackend->SetBlendState(ComputeBlendState(translucent ? BlendMode::Translucent : BlendMode::Opaque));
	mBackend->BindTexture(0, tex);
	mBackend->UpdateBuffer(mVbo, tris.data(), tris.size() * sizeof(Vertex2D));

	const int stride = ComputeVertexStride(kLayout, 3);
	int offsets[3];
	ComputeAttribOffsets(kLayout, 3, offsets);
	const AttribBinding bindings[3] = {
		{0, kLayout[0], offsets[0], false},
		{1, kLayout[1], offsets[1], false},
		{2, kLayout[2], offsets[2], true},
	};
	mBackend->SetVertexBuffer(mVbo, bindings, 3, stride);
	mBackend->Draw(Primitive::Triangles, 0, (int)tris.size());
}

void Renderer2D::DrawTexturedQuad(TextureHandle tex, float x, float y, float w, float h,
                                  float u0, float v0, float u1, float v1,
                                  unsigned char r, unsigned char g, unsigned char b, unsigned char a,
                                  BlendMode blend)
{
	Vertex2D verts[6];
	ComputeQuadVertices(x, y, w, h, u0, v0, u1, v1, r, g, b, a, verts);

	mBackend->SetBlendState(ComputeBlendState(blend));
	mBackend->BindTexture(0, tex);
	mBackend->UpdateBuffer(mVbo, verts, sizeof(verts));

	const int stride = ComputeVertexStride(kLayout, 3);
	int offsets[3];
	ComputeAttribOffsets(kLayout, 3, offsets);
	const AttribBinding bindings[3] = {
		{0, kLayout[0], offsets[0], false},
		{1, kLayout[1], offsets[1], false},
		{2, kLayout[2], offsets[2], true},
	};
	mBackend->SetVertexBuffer(mVbo, bindings, 3, stride);
	mBackend->Draw(Primitive::Triangles, 0, 6);
}

} // namespace hwrender
