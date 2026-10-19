#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>

#include <csc_gles_log.h>

static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;

static EGLDisplay g_egl_display;
static EGLContext g_egl_context;

static const char *rgba8888tonv12_shader_src =
"#version 310 es\n"
"precision highp float;\n"
"precision highp int;\n"
"layout(local_size_x = 8, local_size_y = 8) in;\n"
"uniform sampler2D tex_src;\n"
"uniform int in_width;\n"
"uniform int in_height;\n"
"layout(std430, binding = 0) writeonly buffer NV12Out {\n"
"    uint words[];\n"
"};\n"
"const mat3 RGB2YUV = mat3(\n"
"    0.2627, -0.139630,  0.500000,\n"
"    0.6780, -0.360370, -0.459786,\n"
"    0.0593,  0.500000, -0.040214\n"
");\n"
"vec3 yuvAt(ivec2 p) {\n"
"    return RGB2YUV * texelFetch(tex_src, p, 0).rgb;\n"
"}\n"
"uint pack4(uint a, uint b, uint c, uint d) {\n"
"    return a | (b << 8) | (c << 16) | (d << 24);\n"
"}\n"
"uint y8(float y) {\n"
"    return uint(clamp(16.0 + 219.0 * y, 16.0, 235.0) + 0.5);\n"
"}\n"
"uint c8(float c) {\n"
"    return uint(clamp(128.0 + 224.0 * c, 16.0, 240.0) + 0.5);\n"
"}\n"
"void main() {\n"
"    uvec2 p = gl_GlobalInvocationID.xy * uvec2(4u, 2u);\n"
"    uint w = uint(in_width);\n"
"    uint h = uint(in_height);\n"
"    if (p.x + 4u > w || p.y + 2u > h) return;\n"
"    ivec2 q = ivec2(p);\n"
"    vec3 a00 = yuvAt(q + ivec2(0, 0));\n"
"    vec3 a01 = yuvAt(q + ivec2(1, 0));\n"
"    vec3 a02 = yuvAt(q + ivec2(2, 0));\n"
"    vec3 a03 = yuvAt(q + ivec2(3, 0));\n"
"    vec3 a10 = yuvAt(q + ivec2(0, 1));\n"
"    vec3 a11 = yuvAt(q + ivec2(1, 1));\n"
"    vec3 a12 = yuvAt(q + ivec2(2, 1));\n"
"    vec3 a13 = yuvAt(q + ivec2(3, 1));\n"
"    uint y0 = pack4(y8(a00.x), y8(a01.x), y8(a02.x), y8(a03.x));\n"
"    uint y1 = pack4(y8(a10.x), y8(a11.x), y8(a12.x), y8(a13.x));\n"
"    uint ybytes = w * h;\n"
"    uint yi = (p.y * w + p.x) >> 2;\n"
"    words[yi] = y0;\n"
"    words[yi + (w >> 2)] = y1;\n"
"    float u0 = (a00.y + a01.y + a10.y + a11.y) * 0.25;\n"
"    float v0 = (a00.z + a01.z + a10.z + a11.z) * 0.25;\n"
"    float u1 = (a02.y + a03.y + a12.y + a13.y) * 0.25;\n"
"    float v1 = (a02.z + a03.z + a12.z + a13.z) * 0.25;\n"
"    uint uv = pack4(c8(u0), c8(v0), c8(u1), c8(v1));\n"
"    uint uvi = (ybytes + ((p.y >> 1) * w + p.x)) >> 2;\n"
"    words[uvi] = uv;\n"
"}\n";

// For debug
static const int g_rgba_width = 4096;
static const int g_rgba_height = 2160;
static const char *g_rgba_filename = "4096x2160_8bit.rgba";
static const char *g_nv12_filename = "4096x2160_8bit.nv12";

int main(int argc, char **argv)
{
	// program start
	/// Open test file and read data to CPU side memory.
	FILE *fp = fopen(g_rgba_filename, "rb");
	if (!fp) {
		LOG_E("Failed to open rgba file\n");
		return 1;
	}
	struct stat file_stat = { 0 };
	fstat(fileno(fp), &file_stat);
	LOG_I("Opened file, size: %zu Bytes \n", file_stat.st_size);

	size_t size = g_rgba_width * g_rgba_height * 4;
	unsigned char *pixel_data = (unsigned char *) malloc(size);
	fread(pixel_data, 1, size, fp);

	// OpenGL start

	// void egl_init(void)
	{
		EGLint major, minor;
		g_egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		if (!eglInitialize(g_egl_display, &major, &minor)) {
			LOG_E("eglInitialize failed: 0x%04x\n", eglGetError());
		}
		if (!eglBindAPI(EGL_OPENGL_ES_API)) {
			LOG_E("eglBindAPI failed: 0x%x\n", eglGetError());
		}
		const EGLint config_attrs[] = {
			EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
			EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
			EGL_RED_SIZE, 8,
			EGL_GREEN_SIZE, 8,
			EGL_BLUE_SIZE, 8,
			EGL_ALPHA_SIZE, 8,
			EGL_NONE
		};

		EGLConfig egl_config;
		EGLint num_configs;
		if (!eglChooseConfig(g_egl_display, config_attrs, &egl_config, 1, &num_configs) || num_configs == 0) {
			LOG_E("eglChooseConfig failed: 0x%x\n", eglGetError());
		}
		LOG_I("Get %d EGL configs.\n", num_configs);
		EGLint context_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
		g_egl_context = eglCreateContext(g_egl_display, egl_config, EGL_NO_CONTEXT, context_attrs);
		
		if (!eglMakeCurrent(
			g_egl_display,
			EGL_NO_SURFACE,
			EGL_NO_SURFACE,
			g_egl_context
		)) {
			LOG_E("eglMakeCurrent failed: 0x%x\n", eglGetError());
		}
		LOG_I("EGL version: %d.%d\n", major, minor);
		LOG_I("GLES renderer: %s\n", glGetString(GL_RENDERER));
	}

	// bool egl_check_extension(const EGLDisplay display, const const char *ext_string)
	{
		const char *ext_string = "EGL_KHR_image_base";
		const char *ext_str1 = "EGL_KHR_gl_texture_2D_image";
		const char *ext_str_surfaceless = "EGL_KHR_surfaceless_context";

		const char *egl_extensions;

		egl_extensions = eglQueryString(g_egl_display, EGL_EXTENSIONS);
		if (egl_extensions == NULL) {
			LOG_E("eglQueryString failed: 0x%04x\n", eglGetError());
			eglTerminate(g_egl_display);
			return 1;
			// ret = false;
		}
	
		if (strstr(egl_extensions, ext_string) != NULL) {
			LOG_I("EGL_KHR_image_base: supported\n");
			// ret = true;
		} else {
			LOG_I("EGL_KHR_image_base: not supported\n");
			// ret = false;
		}
		if (strstr(egl_extensions, ext_str1) != NULL) {
			LOG_I("EGL_KHR_gl_texture_2D_image: supported\n");
			// ret = true;
		} else {
			LOG_I("EGL_KHR_gl_texture_2D_image: not supported\n");
			// ret = false;
		}
		if (strstr(egl_extensions, ext_str_surfaceless) != NULL) {
			LOG_I("EGL_KHR_surfaceless_context: supported\n");
			// ret = true;
		} else {
			LOG_I("EGL_KHR_surfaceless_context: not supported\n");
			// ret = false;
		}
	}

	/// Initialize EGL functions
	{
		eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
		eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress("eglDestroyImageKHR");
		glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) eglGetProcAddress("glEGLImageTargetTexture2DOES");

		if (!eglCreateImageKHR || !eglDestroyImageKHR || !glEGLImageTargetTexture2DOES) {
			LOG_E("Required KHR extended functions missing.\n");
			return 1;
		}
		LOG_D("eglCreateImageKHR: %p\neglDestroyImageKHR: %p \nglEGLImageTargetTexture2DOES: %p\n",
		eglCreateImageKHR, eglDestroyImageKHR, glEGLImageTargetTexture2DOES);
	}

	GLuint rgba_tex_id;
	/// Import RGBA data to OpenGL 2D texture
	const size_t width = g_rgba_width;
	const size_t height = g_rgba_height;
	{
		glGenTextures(1, &rgba_tex_id);
		glBindTexture(GL_TEXTURE_2D, rgba_tex_id);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
			width, height, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, pixel_data);

		glBindTexture(GL_TEXTURE_2D, 0);
		LOG_I("Generated OpenGL 2D texture for RGBA image, id: %u\n", rgba_tex_id);
	}

	GLuint ssbo;
	size_t nv12_total_size = (size_t)width * (size_t)height * 3 / 2;
	/// Create output SSBO.
	{

		glGenBuffers(1, &ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
			     nv12_total_size, NULL, GL_STREAM_READ);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
		LOG_I("Created SSBO %u\n", ssbo);
	}

	/// Create shader, compile shader and program.
	GLuint program = glCreateProgram();
	{
		GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
		glShaderSource(shader, 1, &rgba8888tonv12_shader_src, NULL);
		glCompileShader(shader);
		LOG_I("Compiling shader...\n");
		GLint status = GL_NO_ERROR;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
		if (!status) {
			GLint log_len;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
			
			char *log = malloc(log_len > 0 ? (size_t)log_len : 1);
			if (log) {
				glGetShaderInfoLog(shader, log_len, NULL, log);
				LOG_E("Compute Shader compile failed: %s\n", log);
				free(log);
			}

			glDeleteShader(shader);
			return 0;
		}
		LOG_I("Compiled Compute Shader\n");
		status = GL_NO_ERROR;
		glAttachShader(program, shader);
		glLinkProgram(program);
		glGetProgramiv(program, GL_LINK_STATUS, &status);
		if (!status) {
			GLint log_len;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
			
			char *log = malloc(log_len > 0 ? (size_t)log_len : 1);
			if (log) {
				glGetProgramInfoLog(program, log_len, NULL, log);
				LOG_E("Program link failed: %s\n", log);
				free(log);
			}

			glDeleteProgram(program);
			glDeleteShader(shader);
			return 0;
		}
		// Program has linked shader, the shader is no longer needed
		glDetachShader(program, shader);
		glDeleteShader(shader);
	}

	/// Run compute program and fetch the result
	{
		/* NV12 format requires: width % 4 == 0，height % 2 == 0 */
		if (width <= 0 || height <= 0 || (width & 3) || (height & 1)) {
			LOG_E("NV12 requires width%%4==0 and height%%2==0\n");
		}

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, rgba_tex_id);
		glUseProgram(program);
		glUniform1i(glGetUniformLocation(program, "tex_src"), 0);
		glUniform1i(glGetUniformLocation(program, "in_width"), width);
		glUniform1i(glGetUniformLocation(program, "in_height"), height);

		/*
		 * local_size = 8x8，
		 * Each invocation handle 4x2 pixels,
		 * Each workgroup coverred 32x16 pixels.
		 */
		glDispatchCompute((width + 31) / 32,
				  (height + 15) / 16,
				  1);

		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
			GL_BUFFER_UPDATE_BARRIER_BIT);
		
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
		void *mapped = glMapBufferRange(
			GL_SHADER_STORAGE_BUFFER, 0,
			(GLsizeiptr)nv12_total_size, GL_MAP_READ_BIT);
		
		if (!mapped) {
			LOG_E("Unable to fetch mapped data!");
		}
		FILE *fp = fopen(g_nv12_filename, "wb");
		if (fp) {
			fwrite(mapped, 1, nv12_total_size, fp);
			fflush(fp);
			fclose(fp);
			LOG_I("Generated output file\n");
		}
		glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	}

	//EGLint attr[] = {
	//	EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
	//	EGL_NONE
	//};
	//EGLImageKHR image = eglCreateImageKHR(g_egl_display, g_egl_context, 
	//	EGL_GL_TEXTURE_2D_KHR, (EGLClientBuffer)(uintptr_t)tex_id, 
	//	nullptr
	//);
	//if (image == EGL_NO_IMAGE_KHR) {
	//	LOG_E("Failed to create EGLImageKHR\n");
	//}
	//LOG_I("Created EGLImageKHR %p\n", image);
	//eglDestroyImageKHR(g_egl_display, image);

	glDeleteTextures(1, &rgba_tex_id);
	LOG_I("Deleted texture %u\n", rgba_tex_id);
	eglTerminate(g_egl_display);
	// program end
	fclose(fp);
	return 0;
}
