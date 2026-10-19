#ifndef CSC_GLES_GLES_SUPPORT_H
#define CSC_GLES_GLES_SUPPORT_H

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES3/gl31.h>

bool gles_is_ext_supported(const EGLDisplay disp, const char *ext_str);

bool gles_create_compute_program(const char *compute_shader_src, GLuint *out_program);

#endif // CSC_GLES_GLES_SUPPORT_H