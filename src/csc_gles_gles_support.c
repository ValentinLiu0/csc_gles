#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <csc_gles_log.h>

#include "csc_gles_gles_support.h"

static const char *g_egl_extensions;

bool gles_is_ext_supported(const EGLDisplay disp, const char *ext_str)
{
	if (g_egl_extensions == nullptr) {
		g_egl_extensions = eglQueryString(disp, EGL_EXTENSIONS);
		if (g_egl_extensions == NULL) {
			LOG_E("eglQueryString failed: 0x%04x\n", eglGetError());
			return false;
		}
	}
	
	if (strstr(g_egl_extensions, ext_str) != nullptr) {
		LOG_D("Extension %s is supported\n", ext_str);
		return true;
	} else {
		LOG_D("Extension %s is NOT supported\n", ext_str);
	}

	return false;
}

bool gles_create_compute_program(const char *compute_shader_src, GLuint *out_program)
{
	GLuint program = glCreateProgram();
	GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(shader, 1, &compute_shader_src, NULL);
	glCompileShader(shader);
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
		return false;
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
		return false;
	}
	// Program has linked shader, the shader is no longer needed
	glDetachShader(program, shader);
	glDeleteShader(shader);

	*out_program = program;
	LOG_D("Created compute program %u\n", *out_program);
	return true;
}