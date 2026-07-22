// [rc4l] Core-profile OpenGL implementation of IRenderBackend (the hwrender seam). It calls modern
// GL only — VAO/VBO + GLSL, no fixed-function/immediate-mode — so it is legal under a GL 3.3 Core
// context (the profile Apple actually supports above 2.1). The pure value logic (matrix packing,
// blend tables, vertex layout) comes from computation/; this file is just the thin GL glue and is
// therefore verified at runtime via the MCP rather than by unit tests. Selected at startup behind
// the vid_hwrender cvar (Phase 1); until then it is built but unused.
#include "features/hwrender/irenderbackend.h"

#ifndef NO_GL // [rc4l] The GL backend is absent from the dedicated-server (NO_GL) build.
#include "gl/system/gl_system.h"

namespace hwrender
{

namespace
{
GLenum GLPrimitive(Primitive p)
{
	switch (p)
	{
	case Primitive::TriangleStrip: return GL_TRIANGLE_STRIP;
	case Primitive::TriangleFan:   return GL_TRIANGLE_FAN;
	case Primitive::Lines:         return GL_LINES;
	case Primitive::Triangles:
	default:                       return GL_TRIANGLES;
	}
}

GLenum GLBlendFactor(BlendFactor f)
{
	switch (f)
	{
	case BlendFactor::Zero:        return GL_ZERO;
	case BlendFactor::SrcAlpha:    return GL_SRC_ALPHA;
	case BlendFactor::InvSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
	case BlendFactor::DstColor:    return GL_DST_COLOR;
	case BlendFactor::One:
	default:                       return GL_ONE;
	}
}

GLenum GLBlendEquation(BlendEquation e)
{
	return e == BlendEquation::ReverseSubtract ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD;
}

GLenum GLAttribType(AttribType t)
{
	return t == AttribType::UNorm8 ? GL_UNSIGNED_BYTE : GL_FLOAT;
}

// [rc4l] Compile one shader stage; returns 0 and leaves the caller to clean up on failure.
GLuint CompileStage(GLenum stage, const char *src)
{
	GLuint sh = glCreateShader(stage);
	glShaderSource(sh, 1, &src, nullptr);
	glCompileShader(sh);
	GLint ok = GL_FALSE;
	glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if (!ok)
	{
		glDeleteShader(sh);
		return 0;
	}
	return sh;
}

class GLBackend : public IRenderBackend
{
public:
	bool Init() override
	{
		// [rc4l] Core profile requires a bound VAO for any vertex fetch; use one global VAO and
		// re-point its attributes per SetVertexBuffer.
		glGenVertexArrays(1, &mVao);
		glBindVertexArray(mVao);
		return mVao != 0;
	}

	void Shutdown() override
	{
		if (mVao) { glDeleteVertexArrays(1, &mVao); mVao = 0; }
	}

	void BeginFrame(int width, int height) override
	{
		glBindVertexArray(mVao);
		SetViewport(0, 0, width, height);
	}

	void EndFrame() override {}

	BufferHandle CreateBuffer(BufferKind kind, const void *data, size_t bytes, bool dynamic) override
	{
		GLuint id = 0;
		glGenBuffers(1, &id);
		const GLenum target = (kind == BufferKind::Index) ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
		glBindBuffer(target, id);
		glBufferData(target, (GLsizeiptr)bytes, data, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
		return (BufferHandle)id;
	}

	void UpdateBuffer(BufferHandle buffer, const void *data, size_t bytes) override
	{
		glBindBuffer(GL_ARRAY_BUFFER, (GLuint)buffer);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, data, GL_DYNAMIC_DRAW);
	}

	void DestroyBuffer(BufferHandle buffer) override
	{
		GLuint id = (GLuint)buffer;
		if (id) glDeleteBuffers(1, &id);
	}

	TextureHandle CreateTexture(int w, int h, TextureFormat fmt, const void *pixels) override
	{
		GLuint id = 0;
		glGenTextures(1, &id);
		glBindTexture(GL_TEXTURE_2D, id);
		const GLint internal = (fmt == TextureFormat::Alpha8) ? GL_R8 : GL_RGBA8;
		const GLenum layout = (fmt == TextureFormat::Alpha8) ? GL_RED : GL_RGBA;
		glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, layout, GL_UNSIGNED_BYTE, pixels);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		return (TextureHandle)id;
	}

	void DestroyTexture(TextureHandle texture) override
	{
		GLuint id = (GLuint)texture;
		if (id) glDeleteTextures(1, &id);
	}

	ShaderHandle CreateShader(const char *vertexSrc, const char *fragSrc) override
	{
		GLuint vs = CompileStage(GL_VERTEX_SHADER, vertexSrc);
		GLuint fs = CompileStage(GL_FRAGMENT_SHADER, fragSrc);
		if (!vs || !fs)
		{
			if (vs) glDeleteShader(vs);
			if (fs) glDeleteShader(fs);
			return ShaderHandle::None;
		}
		GLuint prog = glCreateProgram();
		glAttachShader(prog, vs);
		glAttachShader(prog, fs);
		glLinkProgram(prog);
		glDeleteShader(vs);
		glDeleteShader(fs);
		GLint ok = GL_FALSE;
		glGetProgramiv(prog, GL_LINK_STATUS, &ok);
		if (!ok)
		{
			glDeleteProgram(prog);
			return ShaderHandle::None;
		}
		return (ShaderHandle)prog;
	}

	void DestroyShader(ShaderHandle shader) override
	{
		GLuint id = (GLuint)shader;
		if (id) glDeleteProgram(id);
	}

	int GetUniformLocation(ShaderHandle shader, const char *name) override
	{
		return glGetUniformLocation((GLuint)shader, name);
	}

	void UseShader(ShaderHandle shader) override { glUseProgram((GLuint)shader); }
	void SetUniformMat4(int location, const Mat4 &m) override { glUniformMatrix4fv(location, 1, GL_FALSE, m.m); }
	void SetUniform4f(int location, float x, float y, float z, float w) override { glUniform4f(location, x, y, z, w); }
	void SetUniform1f(int location, float value) override { glUniform1f(location, value); }
	void SetUniform1i(int location, int value) override { glUniform1i(location, value); }

	void BindTexture(int slot, TextureHandle texture) override
	{
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D, (GLuint)texture);
	}

	void SetBlendState(const BlendState &blend) override
	{
		if (!blend.enabled) { glDisable(GL_BLEND); return; }
		glEnable(GL_BLEND);
		glBlendFunc(GLBlendFactor(blend.src), GLBlendFactor(blend.dst));
		glBlendEquation(GLBlendEquation(blend.equation));
	}

	void SetDepthTest(bool enabled, bool write) override
	{
		if (enabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
		glDepthMask(write ? GL_TRUE : GL_FALSE);
	}

	void SetDepthBias(float factor, float units) override
	{
		if (factor == 0.0f && units == 0.0f)
		{
			glDisable(GL_POLYGON_OFFSET_FILL);
		}
		else
		{
			glEnable(GL_POLYGON_OFFSET_FILL);
		}
		glPolygonOffset(factor, units);
	}

	void SetCull(bool enabled) override
	{
		if (enabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
	}

	void SetViewport(int x, int y, int w, int h) override { glViewport(x, y, w, h); }

	void SetScissor(bool enabled, int x, int y, int w, int h) override
	{
		if (!enabled) { glDisable(GL_SCISSOR_TEST); return; }
		glEnable(GL_SCISSOR_TEST);
		glScissor(x, y, w, h);
	}

	void SetVertexBuffer(BufferHandle buffer, const AttribBinding *attribs, int attribCount, int strideBytes) override
	{
		glBindBuffer(GL_ARRAY_BUFFER, (GLuint)buffer);
		for (int i = 0; i < attribCount; i++)
		{
			const AttribBinding &a = attribs[i];
			glEnableVertexAttribArray(a.location);
			glVertexAttribPointer(a.location, a.format.components, GLAttribType(a.format.type),
				a.normalized ? GL_TRUE : GL_FALSE, strideBytes,
				(const void *)(size_t)a.offsetBytes);
		}
	}

	void SetIndexBuffer(BufferHandle buffer) override { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)buffer); }

	void Draw(Primitive prim, int first, int count) override { glDrawArrays(GLPrimitive(prim), first, count); }

	void DrawIndexed(Primitive prim, int indexCount, int firstIndex) override
	{
		glDrawElements(GLPrimitive(prim), indexCount, GL_UNSIGNED_INT,
			(const void *)(size_t)(firstIndex * sizeof(unsigned int)));
	}

	void SetGammaRamp(const unsigned short[256]) override
	{
		// [rc4l] Display gamma is an OS/window operation, not a GL call; the platform video layer
		// applies the ramp (from gammaramp_compute) via SDL/Win32. No-op in the GL backend.
	}

private:
	GLuint mVao = 0;
};
} // namespace

IRenderBackend *CreateGLBackend()
{
	return new GLBackend();
}

} // namespace hwrender

#else // NO_GL

namespace hwrender
{
// [rc4l] No OpenGL in the dedicated-server build: nothing to create.
IRenderBackend *CreateGLBackend() { return nullptr; }
} // namespace hwrender

#endif // NO_GL
