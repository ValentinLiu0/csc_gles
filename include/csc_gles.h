#ifndef CSC_GLES_H
#define CSC_GLES_H

#include <stdlib.h>

/**
 * Most prebuilt OpenGL ES libraries are not presented the extended functions
 * We should load it dynamically toi use them.
 */
#undef EGL_EGLEXT_PROTOTYPES
#undef GL_GLEXT_PROTOTYPES

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>

#ifdef __cplusplus
extern "C" {
#endif

/// FIXME: Shall we need this?
///**
// * Uniform error code.
// * No error: 0;
// * Linux error: errno + 0;
// * GL error: glGetError() + 20000;
// * EGL error eglGetError() + 40000;
// *
// * GL_NO_ERROR = 0; EGL_SUCCESS = 0x3000;
// */
//typedef unsigned int csc_gles_err;

//static inline csc_gles_err _wrap_gl_err(const GLuint gl_err)
//{
//	return (gl_err + 20000);
//}

//static inline csc_gles_err _wrap_egl_err(const EGLint egl_err)
//{
//	return (egl_err + 40000);
//}

//static inline bool _is_error(const csc_gles_err err) {
//	if (err != 0 && err != _wrap_egl_err(EGL_SUCCESS))
//		return true;

//	return false;
//}

typedef enum csc_gles_buffer_type {
	CSC_GLES_BUFFER_TYPE_RGBA8888,
	CSC_GLES_BUFFER_TYPE_NV12,
	CSC_GLES_BUFFER_TYPE_MAX
} csc_gles_buffer_type_t, buffer_type_t;

typedef struct csc_gles_imported_rgba_buffer {
	GLuint tex_id;
	int width;
	int height;
} csc_gles_imported_rgba_buffer_t, rgba_buffer_t;

typedef struct csc_gles_imported_nv12_buffer {
	GLuint y_tex_id;
	int y_width;
	int y_height;
	GLuint uv_tex_id;
	int uv_width;
	int uv_height;
} csc_gles_imported_nv12_buffer_t, nv12_buffer_t;

typedef struct csc_gles_imported_buffer {
	buffer_type_t type;
	union {
		rgba_buffer_t *rgba_buffer;
		nv12_buffer_t *nv12_buffer;
	} buffer;
} csc_gles_imported_buffer_t;

/**
 * Fetch the working status of csc_gles library.
 * Returns whether the library function can be called.
 * If returned false, call csc_gles_init() function.
 */
bool csc_gles_is_ready(void);
/**
 * Initialize the csc_gles library. This should be called at first.
 * Return the initialization status of library.
 * Return true if initialized successfully.
 */
bool csc_gles_init(void);
/**
 * De-initialize the csc_gles library. Call this if this library is
 * no longer needed to be used. And call csc_gles_init() after next
 * using.
 */
void csc_gles_deinit(void);

bool csc_gles_check_ext(const char *ext_str);

bool csc_gles_import_mem(const csc_gles_buffer_type_t type,
			 const int width, const int height,
			 const void *src,
			 csc_gles_imported_buffer_t **out_buf);
bool csc_gles_import_dmabuf(const csc_gles_buffer_type_t type,
			    const int width, const int height,
			    const int fd,
			    csc_gles_imported_buffer_t **out_buf);
void csc_gles_free_buffer(csc_gles_imported_buffer_t *buf);

bool csc_gles_convert_rgba8888_to_nv12(const csc_gles_imported_buffer_t *buf, GLuint *out_ssbo, size_t *out_sz);

bool csc_gles_ssbo_map(const GLuint ssbo, size_t sz, void **out_data);
bool csc_gles_ssbo_unmap(const GLuint ssbo);

#ifdef __cplusplus
}
#endif

#endif // CSC_GLES_H