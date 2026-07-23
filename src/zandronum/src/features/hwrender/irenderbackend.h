// [rc4l] The hwrender abstraction seam. Every draw path goes through IRenderBackend instead of
// calling OpenGL directly: "make a buffer, set this state, Draw." That same interface is satisfied
// by the Core-profile GL backend now and a Vulkan/Metal backend later, with no engine changes — it
// is the single thing that makes a future Vulkan port a drop-in (see PLAN.md). This header only
// declares the interface + neutral value types; it pulls the pure, unit-tested value logic from
// computation/. No GL/Vulkan headers appear here so the engine can include it freely.
#ifndef ZX_HWRENDER_IRENDERBACKEND_H
#define ZX_HWRENDER_IRENDERBACKEND_H

#include <cstdint>
#include <cstddef>
#include "features/hwrender/computation/matrix_compute.h"
#include "features/hwrender/computation/renderstate_compute.h"
#include "features/hwrender/computation/vertexformat_compute.h"

namespace hwrender
{

// [rc4l] Opaque backend resource handles (0 == none/invalid). The backend owns the real objects.
enum class BufferHandle : uint32_t { None = 0 };
enum class TextureHandle : uint32_t { None = 0 };
enum class ShaderHandle : uint32_t { None = 0 };

enum class BufferKind { Vertex, Index };
enum class Primitive { Triangles, TriangleStrip, TriangleFan, Lines };
enum class TextureFormat { RGBA8, Alpha8 };

// [rc4l] One vertex-attribute binding into the currently-set vertex buffer.
struct AttribBinding
{
	int location;      // shader attribute location
	VertexAttrib format;
	int offsetBytes;   // from ComputeAttribOffsets
	bool normalized;   // integer types read as normalized floats
};

// [rc4l] The backend interface. Backends: GLBackend (now), VKBackend (later). All methods are
// issued between BeginFrame/EndFrame except resource create/destroy, which may happen any time.
class IRenderBackend
{
public:
	virtual ~IRenderBackend() {}

	// Lifecycle -------------------------------------------------------------
	virtual bool Init() = 0;                 // create context state; false on failure
	virtual void Shutdown() = 0;
	virtual void BeginFrame(int width, int height) = 0;
	virtual void EndFrame() = 0;             // present

	// Resources -------------------------------------------------------------
	virtual BufferHandle CreateBuffer(BufferKind kind, const void *data, size_t bytes, bool dynamic) = 0;
	virtual void UpdateBuffer(BufferHandle buffer, const void *data, size_t bytes) = 0;
	virtual void DestroyBuffer(BufferHandle buffer) = 0;

	virtual TextureHandle CreateTexture(int w, int h, TextureFormat fmt, const void *pixels) = 0;
	virtual void DestroyTexture(TextureHandle texture) = 0;

	// vertexSrc/fragSrc are GLSL (or SPIR-V for a Vulkan backend); returns None on compile failure.
	virtual ShaderHandle CreateShader(const char *vertexSrc, const char *fragSrc) = 0;
	virtual void DestroyShader(ShaderHandle shader) = 0;
	virtual int GetUniformLocation(ShaderHandle shader, const char *name) = 0;

	// State (mirrors the neutral types resolved by renderstate_compute) ------
	virtual void UseShader(ShaderHandle shader) = 0;
	virtual void SetUniformMat4(int location, const Mat4 &m) = 0;
	virtual void SetUniform4f(int location, float x, float y, float z, float w) = 0;
	virtual void SetUniform1f(int location, float value) = 0;
	virtual void SetUniform1i(int location, int value) = 0;
	virtual void BindTexture(int slot, TextureHandle texture) = 0;
	virtual void SetBlendState(const BlendState &blend) = 0;
	virtual void SetDepthTest(bool enabled, bool write) = 0;
	// [rc4l] Polygon offset. The legacy renderer applies one per pass; sprite billboards intersect
	// the floor they stand on and need it to survive the depth test.
	virtual void SetDepthBias(float factor, float units) = 0;
	virtual void SetCull(bool enabled) = 0;
	virtual void SetViewport(int x, int y, int w, int h) = 0;
	virtual void SetScissor(bool enabled, int x, int y, int w, int h) = 0;

	// Geometry submission ---------------------------------------------------
	virtual void SetVertexBuffer(BufferHandle buffer, const AttribBinding *attribs, int attribCount, int strideBytes) = 0;
	virtual void SetIndexBuffer(BufferHandle buffer) = 0;
	virtual void Draw(Primitive prim, int first, int count) = 0;
	virtual void DrawIndexed(Primitive prim, int indexCount, int firstIndex) = 0;

	// Display gamma (uses gammaramp_compute); restored to identity on shutdown.
	virtual void SetGammaRamp(const unsigned short ramp[256]) = 0;
};

// [rc4l] Created by the platform layer once a Core-profile context exists. Returns nullptr on
// failure. Implemented in gl_backend.cpp (and later vk_backend.cpp, selected by a cvar).
IRenderBackend *CreateGLBackend();

} // namespace hwrender

#endif // ZX_HWRENDER_IRENDERBACKEND_H
