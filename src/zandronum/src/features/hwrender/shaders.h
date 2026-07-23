// [rc4l] Core-profile GLSL for the hwrender backend's base textured+tinted pipeline (Phase 1/2).
// GLSL 330 core (available under GL 3.3 / Apple 4.1 core). Attribute locations match the
// AttribBinding order the draw layer sets: 0=position(vec3), 1=uv(vec2), 2=color(vec4, normalized).
// Uniforms: uMVP (from matrix_compute), uTexture (sampler), uAlphaThreshold (0 disables the test).
#ifndef ZX_HWRENDER_SHADERS_H
#define ZX_HWRENDER_SHADERS_H

namespace hwrender
{

static const char *const kBaseVertexShader =
	"#version 330 core\n"
	"layout(location = 0) in vec3 aPos;\n"
	"layout(location = 1) in vec2 aUV;\n"
	"layout(location = 2) in vec4 aColor;\n"
	"uniform mat4 uMVP;\n"
	"out vec2 vUV;\n"
	"out vec4 vColor;\n"
	"out float vFogDist;\n"
	"void main()\n"
	"{\n"
	"    vUV = aUV;\n"
	"    vColor = aColor;\n"
	"    gl_Position = uMVP * vec4(aPos, 1.0);\n"
	// [rc4l] Clip-space w is view depth, which is what the legacy shader uses as fogdist for the
	// non-radial case (fogenabled 1/-1). Radial fog would need the world position and camera.
	"    vFogDist = gl_Position.w;\n"
	"}\n";

static const char *const kBaseFragmentShader =
	"#version 330 core\n"
	"in vec2 vUV;\n"
	"in vec4 vColor;\n"
	"in float vFogDist;\n"
	"uniform sampler2D uTexture;\n"
	"uniform float uAlphaThreshold;\n"
	// [rc4l] rgb is the fade colour; a is 0 to disable fog (2D and unfogged sectors).
	"uniform vec4 uFogColor;\n"
	// [rc4l] Premultiplied exactly as the legacy path does: density * (-LOG2E / 64000).
	"uniform float uFogDensity;\n"
	"out vec4 fragColor;\n"
	"void main()\n"
	"{\n"
	"    vec4 c = texture(uTexture, vUV) * vColor;\n"
	"    if (c.a <= uAlphaThreshold) discard;\n"
	"    if (uFogColor.a > 0.0)\n"
	"    {\n"
	"        float f = clamp(exp2(uFogDensity * vFogDist), 0.0, 1.0);\n"
	"        c.rgb = mix(uFogColor.rgb, c.rgb, f);\n"
	"    }\n"
	"    fragColor = c;\n"
	"}\n";

} // namespace hwrender

#endif // ZX_HWRENDER_SHADERS_H
