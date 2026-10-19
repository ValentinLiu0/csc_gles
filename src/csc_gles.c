#include <stdio.h>

#include <csc_gles.h>
#include <csc_gles_log.h>

#include "csc_gles_gles_support.h"
#include "csc_gles_shaders.h"

static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = nullptr;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = nullptr;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = nullptr;

static EGLDisplay g_egl_display;
static EGLContext g_egl_context;

static bool g_allow_khr_ext = false;

bool csc_gles_is_ready(void)
{
	if (!g_egl_display || !g_egl_context) {
		return false;
	}

	if (g_allow_khr_ext &&
		(!eglCreateImageKHR || !eglDestroyImageKHR || !glEGLImageTargetTexture2DOES)
	)
		return false;

	return true;
}

bool csc_gles_init(void)
{
	// Create and initialize EGL display
	EGLint major, minor;
	g_egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (!eglInitialize(g_egl_display, &major, &minor)) {
		LOG_E("eglInitialize failed: 0x%04x\n", eglGetError());
		return false;
	}
	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		LOG_E("eglBindAPI failed: 0x%x\n", eglGetError());
		return false;
	}
	LOG_I("EGL version: %d.%d\n", major, minor);
	
	// Check KHR extensions
	// FIXME: Surfaceless context is not necessary.
	if (!gles_is_ext_supported(g_egl_display, "EGL_KHR_image_base") ||
		!gles_is_ext_supported(g_egl_display, "EGL_KHR_gl_texture_2D_image") ||
		!gles_is_ext_supported(g_egl_display, "EGL_KHR_surfaceless_context")
	) {
		LOG_W("Some required KHR extensions are not supported!");
		return false;
	} else {
		g_allow_khr_ext = true;
	}

	// Create and bind EGL context
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
		return false;
	}
	LOG_D("Get %d EGL configs.\n", num_configs);
	EGLint context_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
	g_egl_context = eglCreateContext(g_egl_display, egl_config, EGL_NO_CONTEXT, context_attrs);
		
	if (!eglMakeCurrent(
		g_egl_display,
		EGL_NO_SURFACE,
		EGL_NO_SURFACE,
		g_egl_context
	)) {
		LOG_E("eglMakeCurrent failed: 0x%x\n", eglGetError());
		return false;
	}
	LOG_I("GLES renderer: %s\n", glGetString(GL_RENDERER));

	// Fetch EGL functions
	if (g_allow_khr_ext) {
		eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
		eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress("eglDestroyImageKHR");
		glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) eglGetProcAddress("glEGLImageTargetTexture2DOES");

		if (!eglCreateImageKHR || !eglDestroyImageKHR || !glEGLImageTargetTexture2DOES) {
			LOG_E("Required KHR extension functions is unreachable, set g_allow_khr_ext to false.\n");
			g_allow_khr_ext = false;
		}
		LOG_D("Founded functions:\n\teglCreateImageKHR: %p\n\teglDestroyImageKHR: %p \n\tglEGLImageTargetTexture2DOES: %p\n",
			eglCreateImageKHR, eglDestroyImageKHR, glEGLImageTargetTexture2DOES);
	}

	return true;
}

void csc_gles_deinit(void)
{
	eglCreateImageKHR = nullptr;
	eglDestroyImageKHR = nullptr;
	glEGLImageTargetTexture2DOES = nullptr;

	eglMakeCurrent(g_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, g_egl_context);
	eglDestroyContext(g_egl_display, g_egl_context);
	eglTerminate(g_egl_display);
}

bool csc_gles_check_ext(const char *ext_str)
{
	return gles_is_ext_supported(g_egl_display, ext_str);
}

bool csc_gles_import_mem(const csc_gles_buffer_type_t type,
			 const int width, const int height,
			 const void *src,
			 csc_gles_imported_buffer_t **out_buf)
{
	if (!out_buf) {
		LOG_E("Invalid pointer %p was passed in.", out_buf);
		return false;
	}
	*out_buf = nullptr;

	csc_gles_imported_buffer_t *buf = (csc_gles_imported_buffer_t *) malloc(sizeof(csc_gles_imported_buffer_t));

	buf->type = type;
	switch (type) {
	case CSC_GLES_BUFFER_TYPE_RGBA8888: {
		buf->buffer.rgba_buffer = (rgba_buffer_t *) malloc(sizeof(rgba_buffer_t));
		buf->buffer.rgba_buffer->width = width;
		buf->buffer.rgba_buffer->height = height;

		// Import data to OpenGL 2D texture
		glGenTextures(1, &buf->buffer.rgba_buffer->tex_id);
		glBindTexture(GL_TEXTURE_2D, buf->buffer.rgba_buffer->tex_id);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
			width, height, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, src);

		glBindTexture(GL_TEXTURE_2D, 0);
		LOG_D("Generated texture %u\n", buf->buffer.rgba_buffer->tex_id);
	}
	break;
	case CSC_GLES_BUFFER_TYPE_NV12: {
		// TODO: finish this.
		buf->buffer.nv12_buffer = (nv12_buffer_t *) malloc(sizeof(nv12_buffer_t));
		LOG_E("We have not implemented this.\n");
		return false;
	}
	break;
	default:
		LOG_E("Unsupported buffer type: %u\n", type);
		return false;
	}

	*out_buf = buf;
	return true;
}

void csc_gles_free_buffer(csc_gles_imported_buffer_t *buf)
{
	csc_gles_buffer_type_t t = buf->type;
	switch (t) {
	case CSC_GLES_BUFFER_TYPE_RGBA8888: {
		glDeleteTextures(1, &buf->buffer.rgba_buffer->tex_id);
		LOG_D("Deleted texture %u\n", buf->buffer.rgba_buffer->tex_id);
		free(buf->buffer.rgba_buffer);
	}
	break;
	case CSC_GLES_BUFFER_TYPE_NV12: {
		// TODO: finish this.
		free(buf->buffer.nv12_buffer);
	}
	break;
	default:
		LOG_E("Unsupported buffer type: %u, nothing to do.\n", t);
	}
	free(buf);
}

bool csc_gles_convert_rgba8888_to_nv12(const csc_gles_imported_buffer_t *buf, GLuint *out_ssbo, size_t *out_sz)
{
	if (!out_ssbo || !out_sz) {
		LOG_W("Invalid output pointer! You may not able to fetch the result.");
	}

	if (buf->type != CSC_GLES_BUFFER_TYPE_RGBA8888) {
		LOG_E("The imported buffer type is not RGBA8888!\n");
		return false;
	}

	int width = buf->buffer.rgba_buffer->width;
	int height = buf->buffer.rgba_buffer->height;
	GLuint rgba_tex_id = buf->buffer.rgba_buffer->tex_id;

	if (width <= 0 || height <= 0 || (width & 3) || (height & 1)) {
		LOG_E("NV12 requires width%%4==0 and height%%2==0!\n");
		return false;
	}

	GLuint program;
	gles_create_compute_program(rgba8888tonv12_compute_shader_src, &program);

	size_t nv12_total_size = (size_t)width * (size_t)height * 3 / 2;
	// Create the SSBO output of shader
	GLuint ssbo;
	glGenBuffers(1, &ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
	glBufferData(GL_SHADER_STORAGE_BUFFER,
		     nv12_total_size, NULL, GL_STREAM_READ);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
	LOG_I("Created SSBO %u\n", ssbo);

	// Set program params
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, rgba_tex_id);
	glUseProgram(program);
	glUniform1i(glGetUniformLocation(program, "tex_src"), 0);
	glUniform1i(glGetUniformLocation(program, "in_width"), width);
	glUniform1i(glGetUniformLocation(program, "in_height"), height);

	// Compute
	/*
	 * local_size = 8x8，
	 * Each invocation handle 4x2 pixels,
	 * Each workgroup coverred 32x16 pixels.
	 */
	glDispatchCompute((width + 31) / 32,
			  (height + 15) / 16,
			  1);

	LOG_D("Converting finished and %zu bytes data were generated\n", nv12_total_size);

	if (out_sz)
		*out_sz = nv12_total_size;
	if (out_ssbo)
		*out_ssbo = ssbo;

	return true;
}

bool csc_gles_ssbo_map(const GLuint ssbo, size_t sz, void **out_data)
{
	if (!out_data) {
		LOG_E("Invalid output pointer %p!", out_data);
		return false;
	}

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
		GL_BUFFER_UPDATE_BARRIER_BIT);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
	void *mapped = glMapBufferRange(
		GL_SHADER_STORAGE_BUFFER, 0,
		(GLsizeiptr)sz, GL_MAP_READ_BIT);
	if (!mapped) {
		LOG_E("Failed to map SSBO to CPU side memory\n");
		return false;
	}

	*out_data = mapped;
	return true;
}

bool csc_gles_ssbo_unmap(const GLuint ssbo)
{
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	
	return (glGetError() == GL_NO_ERROR) ? true : false;
}
