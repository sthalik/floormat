#include "enums.h"
#include <stdarg.h>
#include <stdbool.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

struct minimum_value
{
    GLenum pname;
    const char* name;
    double value;
    bool is_larger_stricter;
    const char* spec;
};

struct minimum_range
{
    GLenum pname;
    const char* name;
    double low, high;
};

#define AT_MOST(pname, value) { pname, #pname, value, false, "GL 3.3" }
#define AT_LEAST(pname, value) { pname, #pname, value, true, "GL 3.3" }
#define EXTENSION_AT_MOST(extension, pname, value) { pname, #pname, value, false, #extension }

static const struct minimum_value minimum_values[] = {
    AT_MOST(GL_MAX_TEXTURE_SIZE, 1024),
    AT_MOST(GL_MAX_CUBE_MAP_TEXTURE_SIZE, 1024),
    AT_MOST(GL_MAX_RENDERBUFFER_SIZE, 1024),
    AT_MOST(GL_MAX_RECTANGLE_TEXTURE_SIZE, 1024),
    AT_MOST(GL_MAX_VERTEX_UNIFORM_COMPONENTS, 1024),
    AT_MOST(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, 1024),
    AT_MOST(GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS, 1024),
    AT_MOST(GL_MAX_3D_TEXTURE_SIZE, 256),
    AT_MOST(GL_MAX_ARRAY_TEXTURE_LAYERS, 256),
    AT_MOST(GL_MAX_GEOMETRY_OUTPUT_VERTICES, 256),
    AT_MOST(GL_MAX_TEXTURE_BUFFER_SIZE, 65536),
    AT_MOST(GL_MAX_UNIFORM_BLOCK_SIZE, 16384),
    AT_MOST(GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS, 50176),
    AT_MOST(GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS, 50176),
    AT_MOST(GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS, 50176),
    AT_MOST(GL_MAX_GEOMETRY_OUTPUT_COMPONENTS, 128),
    AT_MOST(GL_MAX_FRAGMENT_INPUT_COMPONENTS, 128),
    AT_MOST(GL_MAX_VERTEX_OUTPUT_COMPONENTS, 64),
    AT_MOST(GL_MAX_GEOMETRY_INPUT_COMPONENTS, 64),
    AT_MOST(GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS, 64),
    AT_MOST(GL_MAX_VARYING_COMPONENTS, 60),
    AT_MOST(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, 48),
    AT_MOST(GL_MAX_UNIFORM_BUFFER_BINDINGS, 36),
    AT_MOST(GL_MAX_COMBINED_UNIFORM_BLOCKS, 36),
    AT_MOST(GL_MAX_VERTEX_ATTRIBS, 16),
    AT_MOST(GL_MAX_TEXTURE_IMAGE_UNITS, 16),
    AT_MOST(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, 16),
    AT_MOST(GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS, 16),
    AT_MOST(GL_MAX_VERTEX_UNIFORM_BLOCKS, 12),
    AT_MOST(GL_MAX_GEOMETRY_UNIFORM_BLOCKS, 12),
    AT_MOST(GL_MAX_FRAGMENT_UNIFORM_BLOCKS, 12),
    AT_MOST(GL_MAX_CLIP_DISTANCES, 8),
    AT_MOST(GL_MAX_DRAW_BUFFERS, 8),
    AT_MOST(GL_MAX_COLOR_ATTACHMENTS, 8),
    AT_MOST(GL_MAX_PROGRAM_TEXEL_OFFSET, 7),
    AT_LEAST(GL_MIN_PROGRAM_TEXEL_OFFSET, -8),
    AT_MOST(GL_SUBPIXEL_BITS, 4),
    AT_MOST(GL_MAX_SAMPLES, 4),
    AT_MOST(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, 4),
    AT_MOST(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS, 4),
    AT_MOST(GL_MAX_TEXTURE_LOD_BIAS, 2),
    AT_MOST(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS, 1),
    AT_MOST(GL_MAX_SAMPLE_MASK_WORDS, 1),
    AT_MOST(GL_MAX_COLOR_TEXTURE_SAMPLES, 1),
    AT_MOST(GL_MAX_DEPTH_TEXTURE_SAMPLES, 1),
    AT_MOST(GL_MAX_INTEGER_SAMPLES, 1),
    AT_MOST(GL_MAX_SERVER_WAIT_TIMEOUT, 0),
    { GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, "GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT", 256, true, "GL 4.6" },
    EXTENSION_AT_MOST(ARB_ES2_compatibility, GL_MAX_VERTEX_UNIFORM_VECTORS, 256),
    EXTENSION_AT_MOST(ARB_ES2_compatibility, GL_MAX_FRAGMENT_UNIFORM_VECTORS, 256),
    EXTENSION_AT_MOST(ARB_ES2_compatibility, GL_MAX_VARYING_VECTORS, 15),
    EXTENSION_AT_MOST(ARB_framebuffer_no_attachments, GL_MAX_FRAMEBUFFER_WIDTH, 16384),
    EXTENSION_AT_MOST(ARB_framebuffer_no_attachments, GL_MAX_FRAMEBUFFER_HEIGHT, 16384),
    EXTENSION_AT_MOST(ARB_framebuffer_no_attachments, GL_MAX_FRAMEBUFFER_LAYERS, 2048),
    EXTENSION_AT_MOST(ARB_framebuffer_no_attachments, GL_MAX_FRAMEBUFFER_SAMPLES, 4),
    EXTENSION_AT_MOST(ARB_texture_filter_anisotropic, GL_MAX_TEXTURE_MAX_ANISOTROPY, 16),
    EXTENSION_AT_MOST(KHR_debug, GL_MAX_DEBUG_MESSAGE_LENGTH, 1),
    EXTENSION_AT_MOST(KHR_debug, GL_MAX_DEBUG_LOGGED_MESSAGES, 1),
    EXTENSION_AT_MOST(KHR_debug, GL_MAX_DEBUG_GROUP_STACK_DEPTH, 64),
    EXTENSION_AT_MOST(KHR_debug, GL_MAX_LABEL_LENGTH, 256),
    EXTENSION_AT_MOST(ARB_vertex_attrib_binding, GL_MAX_VERTEX_ATTRIB_BINDINGS, 16),
    EXTENSION_AT_MOST(ARB_vertex_attrib_binding, GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET, 2047),
};

#define RANGE(pname, low, high) { pname, #pname, low, high }

static const struct minimum_range minimum_ranges[] = {
    RANGE(GL_POINT_SIZE_RANGE, 1, 1),
    RANGE(GL_ALIASED_LINE_WIDTH_RANGE, 1, 1),
    RANGE(GL_SMOOTH_LINE_WIDTH_RANGE, 1, 1),
};

static const struct minimum_value* find_minimum_value(GLenum pname)
{
    for (size_t i = 0; i < sizeof minimum_values / sizeof *minimum_values; i++)
        if (minimum_values[i].pname == pname)
            return &minimum_values[i];
    return NULL;
}

static const struct minimum_range* find_minimum_range(GLenum pname)
{
    for (size_t i = 0; i < sizeof minimum_ranges / sizeof *minimum_ranges; i++)
        if (minimum_ranges[i].pname == pname)
            return &minimum_ranges[i];
    return NULL;
}

static double stricter(double mesa, double minimum, bool is_larger_stricter)
{
    if (is_larger_stricter)
        return mesa > minimum ? mesa : minimum;
    else
        return mesa < minimum ? mesa : minimum;
}

static unsigned replaced_value_count(GLenum pname)
{
    if (find_minimum_value(pname))
        return 1;
    else if (find_minimum_range(pname))
        return 2;
    else
        return 0;
}

static double reported_value(GLenum pname, unsigned i, double mesa)
{
    const struct minimum_value* m = find_minimum_value(pname);
    if (m)
        return stricter(mesa, m->value, m->is_larger_stricter);
    else
    {
        const struct minimum_range* r = find_minimum_range(pname);
        return i == 0 ? stricter(mesa, r->low, true) : stricter(mesa, r->high, false);
    }
}

static void post_glGetBooleanv(GLenum pname, GLboolean* data)
{
    for (unsigned i = 0, n = replaced_value_count(pname); i < n; i++)
        data[i] = reported_value(pname, i, data[i]) != 0;
}

static void post_glGetIntegerv(GLenum pname, GLint* data)
{
    for (unsigned i = 0, n = replaced_value_count(pname); i < n; i++)
        data[i] = (GLint)reported_value(pname, i, data[i]);
}

static void post_glGetInteger64v(GLenum pname, GLint64* data)
{
    for (unsigned i = 0, n = replaced_value_count(pname); i < n; i++)
        data[i] = (GLint64)reported_value(pname, i, (double)data[i]);
}

static void post_glGetFloatv(GLenum pname, GLfloat* data)
{
    for (unsigned i = 0, n = replaced_value_count(pname); i < n; i++)
        data[i] = (GLfloat)reported_value(pname, i, data[i]);
}

static void post_glGetDoublev(GLenum pname, GLdouble* data)
{
    for (unsigned i = 0, n = replaced_value_count(pname); i < n; i++)
        data[i] = reported_value(pname, i, data[i]);
}

__attribute__((format(printf, 1, 2), noreturn))
static void abort_on_limit_violation(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    fputs("gl33-proxy: ", stderr);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    fflush(stderr);
    abort();
}

static void check_at_most(const char* function, const char* param, long long value, GLenum pname)
{
    const struct minimum_value* m = find_minimum_value(pname);
    if (value > m->value)
        abort_on_limit_violation("%s passed %s = %lld, over %s = %g, the %s minimum", function, param, value,
                                 m->name, m->value, m->spec);
}

static void check_dimension(const char* function, const char* param, GLsizei value, GLint level, GLenum pname)
{
    const struct minimum_value* m = find_minimum_value(pname);
    const int minimum = (int)m->value;
    const int bound = level >= 0 && level < 31 && minimum >> level > 1 ? minimum >> level : 1;
    if (level == 0 && value > bound)
        abort_on_limit_violation("%s passed %s = %d at level 0, over %s = %d, the GL 3.3 minimum", function, param,
                                 value, m->name, minimum);
    else if (level > 0 && value > bound)
        abort_on_limit_violation("%s passed %s = %d at level %d, over %d >> %d = %d, from %s = %d, "
                                 "the GL 3.3 minimum", function, param, value, level, minimum, level, bound,
                                 m->name, minimum);
}

static void check_below(const char* function, const char* param, long long value, GLenum pname)
{
    const struct minimum_value* m = find_minimum_value(pname);
    if (value >= m->value)
        abort_on_limit_violation("%s passed %s = %lld, not below %s = %g, the %s minimum", function, param,
                                 value, m->name, m->value, m->spec);
}

static void check_layer(const char* function, const char* param, GLint layer)
{
    const struct minimum_value* layers = find_minimum_value(GL_MAX_ARRAY_TEXTURE_LAYERS);
    const struct minimum_value* depth = find_minimum_value(GL_MAX_3D_TEXTURE_SIZE);
    if (layer >= layers->value && layer >= depth->value)
        abort_on_limit_violation("%s passed %s = %d, not below %s = %g or %s = %g, the GL 3.3 minimums", function,
                                 param, layer, layers->name, layers->value, depth->name, depth->value);
}

static void check_color_attachment(const char* function, const char* param, GLenum attachment)
{
    const struct minimum_value* m = find_minimum_value(GL_MAX_COLOR_ATTACHMENTS);
    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT31 &&
        attachment - GL_COLOR_ATTACHMENT0 >= m->value)
        abort_on_limit_violation("%s passed %s = GL_COLOR_ATTACHMENT0 + %u, not below %s = %g, the GL 3.3 minimum",
                                 function, param, attachment - GL_COLOR_ATTACHMENT0, m->name, m->value);
}

static void check_texture_size(const char* function, GLenum target, GLint level, GLsizei width, GLsizei height,
                               GLsizei depth)
{
    switch (target)
    {
    case GL_TEXTURE_1D:
        check_dimension(function, "width", width, level, GL_MAX_TEXTURE_SIZE);
        break;
    case GL_TEXTURE_1D_ARRAY:
        check_dimension(function, "width", width, level, GL_MAX_TEXTURE_SIZE);
        check_at_most(function, "height", height, GL_MAX_ARRAY_TEXTURE_LAYERS);
        break;
    case GL_TEXTURE_2D:
    case GL_TEXTURE_2D_MULTISAMPLE:
        check_dimension(function, "width", width, level, GL_MAX_TEXTURE_SIZE);
        check_dimension(function, "height", height, level, GL_MAX_TEXTURE_SIZE);
        break;
    case GL_TEXTURE_2D_ARRAY:
    case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        check_dimension(function, "width", width, level, GL_MAX_TEXTURE_SIZE);
        check_dimension(function, "height", height, level, GL_MAX_TEXTURE_SIZE);
        check_at_most(function, "depth", depth, GL_MAX_ARRAY_TEXTURE_LAYERS);
        break;
    case GL_TEXTURE_RECTANGLE:
        check_dimension(function, "width", width, level, GL_MAX_RECTANGLE_TEXTURE_SIZE);
        check_dimension(function, "height", height, level, GL_MAX_RECTANGLE_TEXTURE_SIZE);
        break;
    case GL_TEXTURE_CUBE_MAP:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        check_dimension(function, "width", width, level, GL_MAX_CUBE_MAP_TEXTURE_SIZE);
        check_dimension(function, "height", height, level, GL_MAX_CUBE_MAP_TEXTURE_SIZE);
        break;
    case GL_TEXTURE_CUBE_MAP_ARRAY:
        check_dimension(function, "width", width, level, GL_MAX_CUBE_MAP_TEXTURE_SIZE);
        check_dimension(function, "height", height, level, GL_MAX_CUBE_MAP_TEXTURE_SIZE);
        check_at_most(function, "depth", depth, GL_MAX_ARRAY_TEXTURE_LAYERS);
        break;
    case GL_TEXTURE_3D:
        check_dimension(function, "width", width, level, GL_MAX_3D_TEXTURE_SIZE);
        check_dimension(function, "height", height, level, GL_MAX_3D_TEXTURE_SIZE);
        check_dimension(function, "depth", depth, level, GL_MAX_3D_TEXTURE_SIZE);
        break;
    case GL_PROXY_TEXTURE_1D:
    case GL_PROXY_TEXTURE_1D_ARRAY:
    case GL_PROXY_TEXTURE_2D:
    case GL_PROXY_TEXTURE_2D_ARRAY:
    case GL_PROXY_TEXTURE_2D_MULTISAMPLE:
    case GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY:
    case GL_PROXY_TEXTURE_RECTANGLE:
    case GL_PROXY_TEXTURE_CUBE_MAP:
    case GL_PROXY_TEXTURE_CUBE_MAP_ARRAY:
    case GL_PROXY_TEXTURE_3D:
        abort_on_limit_violation("%s passed target = 0x%04X, a proxy texture target, and gl33-proxy doesn't lower "
                                 "the limits proxy textures test", function, target);
    default:
        break;
    }
}

static bool is_integer_format(GLenum format)
{
    switch (format)
    {
    case GL_R8I: case GL_R8UI: case GL_R16I: case GL_R16UI: case GL_R32I: case GL_R32UI:
    case GL_RG8I: case GL_RG8UI: case GL_RG16I: case GL_RG16UI: case GL_RG32I: case GL_RG32UI:
    case GL_RGB8I: case GL_RGB8UI: case GL_RGB16I: case GL_RGB16UI: case GL_RGB32I: case GL_RGB32UI:
    case GL_RGBA8I: case GL_RGBA8UI: case GL_RGBA16I: case GL_RGBA16UI: case GL_RGBA32I: case GL_RGBA32UI:
    case GL_RGB10_A2UI:
        return true;
    default:
        return false;
    }
}

static bool is_depth_or_stencil_format(GLenum format)
{
    switch (format)
    {
    case GL_DEPTH_COMPONENT: case GL_DEPTH_COMPONENT16: case GL_DEPTH_COMPONENT24: case GL_DEPTH_COMPONENT32:
    case GL_DEPTH_COMPONENT32F: case GL_DEPTH_STENCIL: case GL_DEPTH24_STENCIL8: case GL_DEPTH32F_STENCIL8:
    case GL_STENCIL_INDEX: case GL_STENCIL_INDEX1: case GL_STENCIL_INDEX4: case GL_STENCIL_INDEX8:
    case GL_STENCIL_INDEX16:
        return true;
    default:
        return false;
    }
}

static void check_texture_samples(const char* function, GLsizei samples, GLenum internalformat)
{
    if (is_integer_format(internalformat))
        check_at_most(function, "samples", samples, GL_MAX_INTEGER_SAMPLES);
    else if (is_depth_or_stencil_format(internalformat))
        check_at_most(function, "samples", samples, GL_MAX_DEPTH_TEXTURE_SAMPLES);
    else
        check_at_most(function, "samples", samples, GL_MAX_COLOR_TEXTURE_SAMPLES);
}

static void resolve_mesa_function(void* destination, const char* name)
{
    void* f = (void*)GetProcAddress(GetModuleHandleA("opengl32-mesa.dll"), name);
    if (!f)
        f = (void*)mesa_get_proc_address(name);
    if (!f)
        fail("opengl32-mesa.dll has no %s", name);
    memcpy(destination, &f, sizeof f);
}

static struct
{
    void (WINAPI *GetIntegerv)(GLenum, GLint*);
    void (WINAPI *GetTextureParameteriv)(GLuint, GLenum, GLint*);
    void (WINAPI *GetProgramiv)(GLuint, GLenum, GLint*);
    void (WINAPI *GetActiveAttrib)(GLuint, GLuint, GLsizei, GLsizei*, GLint*, GLenum*, GLchar*);
    GLint (WINAPI *GetAttribLocation)(GLuint, const GLchar*);
    void (WINAPI *GetActiveUniformBlockiv)(GLuint, GLuint, GLenum, GLint*);
    void (WINAPI *GetActiveUniformBlockName)(GLuint, GLuint, GLsizei, GLsizei*, GLchar*);
    void (WINAPI *GetProgramInterfaceiv)(GLuint, GLenum, GLenum, GLint*);
    void (WINAPI *GetProgramResourceiv)(GLuint, GLenum, GLuint, GLsizei, const GLenum*, GLsizei, GLsizei*, GLint*);
    void (WINAPI *GetProgramResourceName)(GLuint, GLenum, GLuint, GLsizei, GLsizei*, GLchar*);
    GLint (WINAPI *GetUniformLocation)(GLuint, const GLchar*);
    void (WINAPI *GetProgramPipelineiv)(GLuint, GLenum, GLint*);
} mesa_queries;

static void ensure_mesa_queries(void)
{
    if (!mesa_queries.GetIntegerv)
    {
        resolve_mesa_function(&mesa_queries.GetTextureParameteriv, "glGetTextureParameteriv");
        resolve_mesa_function(&mesa_queries.GetProgramiv, "glGetProgramiv");
        resolve_mesa_function(&mesa_queries.GetActiveAttrib, "glGetActiveAttrib");
        resolve_mesa_function(&mesa_queries.GetAttribLocation, "glGetAttribLocation");
        resolve_mesa_function(&mesa_queries.GetActiveUniformBlockiv, "glGetActiveUniformBlockiv");
        resolve_mesa_function(&mesa_queries.GetActiveUniformBlockName, "glGetActiveUniformBlockName");
        resolve_mesa_function(&mesa_queries.GetProgramInterfaceiv, "glGetProgramInterfaceiv");
        resolve_mesa_function(&mesa_queries.GetProgramResourceiv, "glGetProgramResourceiv");
        resolve_mesa_function(&mesa_queries.GetProgramResourceName, "glGetProgramResourceName");
        resolve_mesa_function(&mesa_queries.GetUniformLocation, "glGetUniformLocation");
        resolve_mesa_function(&mesa_queries.GetProgramPipelineiv, "glGetProgramPipelineiv");
        resolve_mesa_function(&mesa_queries.GetIntegerv, "glGetIntegerv");
    }
}

static GLenum texture_target(GLuint texture)
{
    GLint target = 0;
    ensure_mesa_queries();
    mesa_queries.GetTextureParameteriv(texture, GL_TEXTURE_TARGET, &target);
    return (GLenum)target;
}

static GLenum mip_level_size_limit(GLenum target)
{
    switch (target)
    {
    case GL_TEXTURE_1D: case GL_TEXTURE_2D: case GL_TEXTURE_1D_ARRAY: case GL_TEXTURE_2D_ARRAY:
        return GL_MAX_TEXTURE_SIZE;
    case GL_TEXTURE_3D:
        return GL_MAX_3D_TEXTURE_SIZE;
    case GL_TEXTURE_CUBE_MAP: case GL_TEXTURE_CUBE_MAP_ARRAY:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X: case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y: case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z: case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        return GL_MAX_CUBE_MAP_TEXTURE_SIZE;
    default:
        return 0;
    }
}

static void check_level(const char* function, GLenum target, GLint level)
{
    const GLenum pname = mip_level_size_limit(target);
    if (pname)
    {
        const struct minimum_value* m = find_minimum_value(pname);
        int max_level = 0;
        while (1 << (max_level + 1) <= m->value)
            max_level++;
        if (level > max_level)
            abort_on_limit_violation("%s passed level = %d, over %d, the base 2 logarithm of %s = %g, the GL 3.3 "
                                     "minimum", function, level, max_level, m->name, m->value);
    }
}

static void pre_glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border,
                             GLenum format, GLenum type, const void* pixels)
{
    check_texture_size("glTexImage1D", target, level, width, 1, 1);
}

static void pre_glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                             GLint border, GLenum format, GLenum type, const void* pixels)
{
    check_texture_size("glTexImage2D", target, level, width, height, 1);
}

static void pre_glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                             GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels)
{
    check_texture_size("glTexImage3D", target, level, width, height, depth);
}

static void pre_glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
                                 GLsizei width, GLint border)
{
    check_texture_size("glCopyTexImage1D", target, level, width, 1, 1);
}

static void pre_glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
                                 GLsizei width, GLsizei height, GLint border)
{
    check_texture_size("glCopyTexImage2D", target, level, width, height, 1);
}

static void pre_glCompressedTexImage1D(GLenum target, GLint level, GLenum internalformat, GLsizei width,
                                       GLint border, GLsizei imageSize, const void* data)
{
    check_texture_size("glCompressedTexImage1D", target, level, width, 1, 1);
}

static void pre_glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width,
                                       GLsizei height, GLint border, GLsizei imageSize, const void* data)
{
    check_texture_size("glCompressedTexImage2D", target, level, width, height, 1);
}

static void pre_glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width,
                                       GLsizei height, GLsizei depth, GLint border, GLsizei imageSize,
                                       const void* data)
{
    check_texture_size("glCompressedTexImage3D", target, level, width, height, depth);
}

static void pre_glTexImage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
                                        GLsizei height, GLboolean fixedsamplelocations)
{
    check_texture_size("glTexImage2DMultisample", target, 0, width, height, 1);
    check_texture_samples("glTexImage2DMultisample", samples, internalformat);
}

static void pre_glTexImage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
                                        GLsizei height, GLsizei depth, GLboolean fixedsamplelocations)
{
    check_texture_size("glTexImage3DMultisample", target, 0, width, height, depth);
    check_texture_samples("glTexImage3DMultisample", samples, internalformat);
}

static void pre_glTexStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width)
{
    check_texture_size("glTexStorage1D", target, 0, width, 1, 1);
}

static void pre_glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
                               GLsizei height)
{
    check_texture_size("glTexStorage2D", target, 0, width, height, 1);
}

static void pre_glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
                               GLsizei height, GLsizei depth)
{
    check_texture_size("glTexStorage3D", target, 0, width, height, depth);
}

static void pre_glTexStorage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
                                          GLsizei height, GLboolean fixedsamplelocations)
{
    check_texture_size("glTexStorage2DMultisample", target, 0, width, height, 1);
    check_texture_samples("glTexStorage2DMultisample", samples, internalformat);
}

static void pre_glTexStorage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
                                          GLsizei height, GLsizei depth, GLboolean fixedsamplelocations)
{
    check_texture_size("glTexStorage3DMultisample", target, 0, width, height, depth);
    check_texture_samples("glTexStorage3DMultisample", samples, internalformat);
}

static void pre_glTextureStorage1D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width)
{
    check_texture_size("glTextureStorage1D", texture_target(texture), 0, width, 1, 1);
}

static void pre_glTextureStorage2D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width,
                                   GLsizei height)
{
    check_texture_size("glTextureStorage2D", texture_target(texture), 0, width, height, 1);
}

static void pre_glTextureStorage3D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width,
                                   GLsizei height, GLsizei depth)
{
    check_texture_size("glTextureStorage3D", texture_target(texture), 0, width, height, depth);
}

static void pre_glTextureStorage2DMultisample(GLuint texture, GLsizei samples, GLenum internalformat,
                                              GLsizei width, GLsizei height, GLboolean fixedsamplelocations)
{
    check_texture_size("glTextureStorage2DMultisample", texture_target(texture), 0, width, height, 1);
    check_texture_samples("glTextureStorage2DMultisample", samples, internalformat);
}

static void pre_glTextureStorage3DMultisample(GLuint texture, GLsizei samples, GLenum internalformat,
                                              GLsizei width, GLsizei height, GLsizei depth,
                                              GLboolean fixedsamplelocations)
{
    check_texture_size("glTextureStorage3DMultisample", texture_target(texture), 0, width, height, depth);
    check_texture_samples("glTextureStorage3DMultisample", samples, internalformat);
}

static void pre_glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
                                       GLint level, GLint zoffset)
{
    check_color_attachment("glFramebufferTexture3D", "attachment", attachment);
    if (texture)
        check_below("glFramebufferTexture3D", "zoffset", zoffset, GL_MAX_3D_TEXTURE_SIZE);
}

static void pre_glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level,
                                          GLint layer)
{
    check_color_attachment("glFramebufferTextureLayer", "attachment", attachment);
    if (texture)
        check_layer("glFramebufferTextureLayer", "layer", layer);
}

static void pre_glNamedFramebufferTextureLayer(GLuint framebuffer, GLenum attachment, GLuint texture,
                                               GLint level, GLint layer)
{
    check_color_attachment("glNamedFramebufferTextureLayer", "attachment", attachment);
    if (texture)
        check_layer("glNamedFramebufferTextureLayer", "layer", layer);
}

static void check_renderbuffer(const char* function, GLsizei samples, GLenum internalformat, GLsizei width,
                               GLsizei height)
{
    check_at_most(function, "width", width, GL_MAX_RENDERBUFFER_SIZE);
    check_at_most(function, "height", height, GL_MAX_RENDERBUFFER_SIZE);
    if (is_integer_format(internalformat))
        check_at_most(function, "samples", samples, GL_MAX_INTEGER_SAMPLES);
    else
        check_at_most(function, "samples", samples, GL_MAX_SAMPLES);
}

static void pre_glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
    check_renderbuffer("glRenderbufferStorage", 0, internalformat, width, height);
}

static void pre_glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat,
                                                 GLsizei width, GLsizei height)
{
    check_renderbuffer("glRenderbufferStorageMultisample", samples, internalformat, width, height);
}

static void pre_glNamedRenderbufferStorage(GLuint renderbuffer, GLenum internalformat, GLsizei width,
                                           GLsizei height)
{
    check_renderbuffer("glNamedRenderbufferStorage", 0, internalformat, width, height);
}

static void pre_glNamedRenderbufferStorageMultisample(GLuint renderbuffer, GLsizei samples, GLenum internalformat,
                                                      GLsizei width, GLsizei height)
{
    check_renderbuffer("glNamedRenderbufferStorageMultisample", samples, internalformat, width, height);
}

static void pre_glActiveTexture(GLenum texture)
{
    if (texture >= GL_TEXTURE0)
        check_below("glActiveTexture", "texture - GL_TEXTURE0", texture - GL_TEXTURE0,
                    GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
}

static void pre_glBindSampler(GLuint unit, GLuint sampler)
{
    check_below("glBindSampler", "unit", unit, GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
}

static void pre_glBindTextureUnit(GLuint unit, GLuint texture)
{
    check_below("glBindTextureUnit", "unit", unit, GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
}

static void pre_glBindTextures(GLuint first, GLsizei count, const GLuint* textures)
{
    check_at_most("glBindTextures", "first + count", (long long)first + count,
                  GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
}

static void pre_glBindSamplers(GLuint first, GLsizei count, const GLuint* samplers)
{
    check_at_most("glBindSamplers", "first + count", (long long)first + count,
                  GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
}

static void check_color_attachments(const char* function, const char* param, GLsizei count,
                                    const GLenum* attachments)
{
    char name[64];
    for (GLsizei i = 0; attachments && i < count; i++)
    {
        snprintf(name, sizeof name, "%s[%d]", param, i);
        check_color_attachment(function, name, attachments[i]);
    }
}

static void pre_glDrawBuffers(GLsizei n, const GLenum* bufs)
{
    check_at_most("glDrawBuffers", "n", n, GL_MAX_DRAW_BUFFERS);
    check_color_attachments("glDrawBuffers", "bufs", n, bufs);
}

static void pre_glNamedFramebufferDrawBuffers(GLuint framebuffer, GLsizei n, const GLenum* bufs)
{
    check_at_most("glNamedFramebufferDrawBuffers", "n", n, GL_MAX_DRAW_BUFFERS);
    check_color_attachments("glNamedFramebufferDrawBuffers", "bufs", n, bufs);
}

static void pre_glDrawBuffer(GLenum buf)
{
    check_color_attachment("glDrawBuffer", "buf", buf);
}

static void pre_glReadBuffer(GLenum src)
{
    check_color_attachment("glReadBuffer", "src", src);
}

static void pre_glNamedFramebufferDrawBuffer(GLuint framebuffer, GLenum buf)
{
    check_color_attachment("glNamedFramebufferDrawBuffer", "buf", buf);
}

static void pre_glNamedFramebufferReadBuffer(GLuint framebuffer, GLenum src)
{
    check_color_attachment("glNamedFramebufferReadBuffer", "src", src);
}

static void pre_glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level)
{
    check_color_attachment("glFramebufferTexture", "attachment", attachment);
}

static void pre_glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
                                       GLint level)
{
    check_color_attachment("glFramebufferTexture1D", "attachment", attachment);
}

static void pre_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
                                       GLint level)
{
    check_color_attachment("glFramebufferTexture2D", "attachment", attachment);
}

static void pre_glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget,
                                          GLuint renderbuffer)
{
    check_color_attachment("glFramebufferRenderbuffer", "attachment", attachment);
}

static void pre_glNamedFramebufferTexture(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level)
{
    check_color_attachment("glNamedFramebufferTexture", "attachment", attachment);
}

static void pre_glNamedFramebufferRenderbuffer(GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget,
                                               GLuint renderbuffer)
{
    check_color_attachment("glNamedFramebufferRenderbuffer", "attachment", attachment);
}

static void pre_glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname,
                                                      GLint* params)
{
    check_color_attachment("glGetFramebufferAttachmentParameteriv", "attachment", attachment);
}

static void pre_glGetNamedFramebufferAttachmentParameteriv(GLuint framebuffer, GLenum attachment, GLenum pname,
                                                           GLint* params)
{
    check_color_attachment("glGetNamedFramebufferAttachmentParameteriv", "attachment", attachment);
}

static void pre_glInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum* attachments)
{
    check_color_attachments("glInvalidateFramebuffer", "attachments", numAttachments, attachments);
}

static void pre_glInvalidateSubFramebuffer(GLenum target, GLsizei numAttachments, const GLenum* attachments,
                                           GLint x, GLint y, GLsizei width, GLsizei height)
{
    check_color_attachments("glInvalidateSubFramebuffer", "attachments", numAttachments, attachments);
}

static void pre_glInvalidateNamedFramebufferData(GLuint framebuffer, GLsizei numAttachments,
                                                 const GLenum* attachments)
{
    check_color_attachments("glInvalidateNamedFramebufferData", "attachments", numAttachments, attachments);
}

static void pre_glInvalidateNamedFramebufferSubData(GLuint framebuffer, GLsizei numAttachments,
                                                    const GLenum* attachments, GLint x, GLint y, GLsizei width,
                                                    GLsizei height)
{
    check_color_attachments("glInvalidateNamedFramebufferSubData", "attachments", numAttachments, attachments);
}

static void check_clear_buffer(const char* function, GLenum buffer, GLint drawbuffer)
{
    if (buffer == GL_COLOR)
        check_below(function, "drawbuffer", drawbuffer, GL_MAX_DRAW_BUFFERS);
}

static void pre_glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint* value)
{
    check_clear_buffer("glClearBufferiv", buffer, drawbuffer);
}

static void pre_glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint* value)
{
    check_clear_buffer("glClearBufferuiv", buffer, drawbuffer);
}

static void pre_glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat* value)
{
    check_clear_buffer("glClearBufferfv", buffer, drawbuffer);
}

static void pre_glClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint* value)
{
    check_clear_buffer("glClearNamedFramebufferiv", buffer, drawbuffer);
}

static void pre_glClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                           const GLuint* value)
{
    check_clear_buffer("glClearNamedFramebufferuiv", buffer, drawbuffer);
}

static void pre_glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                          const GLfloat* value)
{
    check_clear_buffer("glClearNamedFramebufferfv", buffer, drawbuffer);
}

static void check_indexed_state(const char* function, GLenum target, GLuint index)
{
    switch (target)
    {
    case GL_UNIFORM_BUFFER_BINDING:
    case GL_UNIFORM_BUFFER_START:
    case GL_UNIFORM_BUFFER_SIZE:
        check_below(function, "index", index, GL_MAX_UNIFORM_BUFFER_BINDINGS);
        break;
    case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
    case GL_TRANSFORM_FEEDBACK_BUFFER_START:
    case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
        check_below(function, "index", index, GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS);
        break;
    case GL_BLEND:
    case GL_COLOR_WRITEMASK:
        check_below(function, "index", index, GL_MAX_DRAW_BUFFERS);
        break;
    case GL_SAMPLE_MASK_VALUE:
        check_below(function, "index", index, GL_MAX_SAMPLE_MASK_WORDS);
        break;
    default:
        break;
    }
}

static void pre_glGetBooleani_v(GLenum target, GLuint index, GLboolean* data)
{
    check_indexed_state("glGetBooleani_v", target, index);
}

static void pre_glGetIntegeri_v(GLenum target, GLuint index, GLint* data)
{
    check_indexed_state("glGetIntegeri_v", target, index);
}

static void pre_glGetInteger64i_v(GLenum target, GLuint index, GLint64* data)
{
    check_indexed_state("glGetInteger64i_v", target, index);
}

static void pre_glEnablei(GLenum target, GLuint index)
{
    check_indexed_state("glEnablei", target, index);
}

static void pre_glDisablei(GLenum target, GLuint index)
{
    check_indexed_state("glDisablei", target, index);
}

static void pre_glIsEnabledi(GLenum target, GLuint index)
{
    check_indexed_state("glIsEnabledi", target, index);
}

static void pre_glColorMaski(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{
    check_below("glColorMaski", "index", index, GL_MAX_DRAW_BUFFERS);
}

static void pre_glBlendFunci(GLuint buf, GLenum src, GLenum dst)
{
    check_below("glBlendFunci", "buf", buf, GL_MAX_DRAW_BUFFERS);
}

static void pre_glBlendFuncSeparatei(GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
    check_below("glBlendFuncSeparatei", "buf", buf, GL_MAX_DRAW_BUFFERS);
}

static void pre_glBlendEquationi(GLuint buf, GLenum mode)
{
    check_below("glBlendEquationi", "buf", buf, GL_MAX_DRAW_BUFFERS);
}

static void pre_glBlendEquationSeparatei(GLuint buf, GLenum modeRGB, GLenum modeAlpha)
{
    check_below("glBlendEquationSeparatei", "buf", buf, GL_MAX_DRAW_BUFFERS);
}

static void pre_glBindFragDataLocation(GLuint program, GLuint color, const GLchar* name)
{
    check_below("glBindFragDataLocation", "color", color, GL_MAX_DRAW_BUFFERS);
}

static void pre_glBindFragDataLocationIndexed(GLuint program, GLuint colorNumber, GLuint index, const GLchar* name)
{
    if (index == 0)
        check_below("glBindFragDataLocationIndexed", "colorNumber", colorNumber, GL_MAX_DRAW_BUFFERS);
    else if (index == 1)
        check_below("glBindFragDataLocationIndexed", "colorNumber", colorNumber, GL_MAX_DUAL_SOURCE_DRAW_BUFFERS);
}

static GLenum buffer_binding_limit(GLenum target)
{
    switch (target)
    {
    case GL_UNIFORM_BUFFER:
        return GL_MAX_UNIFORM_BUFFER_BINDINGS;
    case GL_TRANSFORM_FEEDBACK_BUFFER:
        return GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS;
    default:
        return 0;
    }
}

static void check_uniform_buffer_offset(const char* function, const char* param, GLintptr offset)
{
    const struct minimum_value* m = find_minimum_value(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT);
    if (offset % (GLintptr)m->value)
        abort_on_limit_violation("%s passed %s = %td, not a multiple of %s = %g, the largest value GL 4.6 allows",
                                 function, param, offset, m->name, m->value);
}

static void pre_glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
    const GLenum limit = buffer_binding_limit(target);
    if (limit)
        check_below("glBindBufferBase", "index", index, limit);
}

static void pre_glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    const GLenum limit = buffer_binding_limit(target);
    if (limit)
        check_below("glBindBufferRange", "index", index, limit);
    if (target == GL_UNIFORM_BUFFER && buffer)
        check_uniform_buffer_offset("glBindBufferRange", "offset", offset);
}

static void pre_glBindBuffersBase(GLenum target, GLuint first, GLsizei count, const GLuint* buffers)
{
    const GLenum limit = buffer_binding_limit(target);
    if (limit)
        check_at_most("glBindBuffersBase", "first + count", (long long)first + count, limit);
}

static void pre_glBindBuffersRange(GLenum target, GLuint first, GLsizei count, const GLuint* buffers,
                                   const GLintptr* offsets, const GLsizeiptr* sizes)
{
    const GLenum limit = buffer_binding_limit(target);
    char name[64];
    if (limit)
        check_at_most("glBindBuffersRange", "first + count", (long long)first + count, limit);
    for (GLsizei i = 0; target == GL_UNIFORM_BUFFER && buffers && offsets && i < count; i++)
    {
        snprintf(name, sizeof name, "offsets[%d]", i);
        if (buffers[i])
            check_uniform_buffer_offset("glBindBuffersRange", name, offsets[i]);
    }
}

static void pre_glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    check_below("glUniformBlockBinding", "uniformBlockBinding", uniformBlockBinding,
                GL_MAX_UNIFORM_BUFFER_BINDINGS);
}

static void pre_glTransformFeedbackVaryings(GLuint program, GLsizei count, const GLchar* const* varyings,
                                            GLenum bufferMode)
{
    if (bufferMode == GL_SEPARATE_ATTRIBS)
        check_at_most("glTransformFeedbackVaryings", "count", count, GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS);
}

static void pre_glSampleMaski(GLuint maskNumber, GLbitfield mask)
{
    check_below("glSampleMaski", "maskNumber", maskNumber, GL_MAX_SAMPLE_MASK_WORDS);
}

static void check_range_high(const char* function, const char* param, double value, GLenum pname)
{
    const struct minimum_range* r = find_minimum_range(pname);
    if (value > r->high)
        abort_on_limit_violation("%s passed %s = %g, over %g, the high end of %s = (%g, %g), the GL 3.3 minimum",
                                 function, param, value, r->high, r->name, r->low, r->high);
}

static void pre_glLineWidth(GLfloat width)
{
    check_range_high("glLineWidth", "width", width, GL_ALIASED_LINE_WIDTH_RANGE);
}

static void pre_glPointSize(GLfloat size)
{
    check_range_high("glPointSize", "size", size, GL_POINT_SIZE_RANGE);
}

static void check_lod_bias(const char* function, GLenum pname, double bias)
{
    const struct minimum_value* m = find_minimum_value(GL_MAX_TEXTURE_LOD_BIAS);
    if (pname == GL_TEXTURE_LOD_BIAS && (bias > m->value || bias < -m->value))
        abort_on_limit_violation("%s passed GL_TEXTURE_LOD_BIAS = %g, outside [-%g, %g], from %s = %g, "
                                 "the GL 3.3 minimum", function, bias, m->value, m->value, m->name, m->value);
}

static void pre_glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
    check_lod_bias("glTexParameterf", pname, param);
}

static void pre_glTexParameterfv(GLenum target, GLenum pname, const GLfloat* params)
{
    check_lod_bias("glTexParameterfv", pname, params ? params[0] : 0);
}

static void pre_glTexParameteri(GLenum target, GLenum pname, GLint param)
{
    check_lod_bias("glTexParameteri", pname, param);
}

static void pre_glTexParameteriv(GLenum target, GLenum pname, const GLint* params)
{
    check_lod_bias("glTexParameteriv", pname, params ? params[0] : 0);
}

static void pre_glTexParameterIiv(GLenum target, GLenum pname, const GLint* params)
{
    check_lod_bias("glTexParameterIiv", pname, params ? params[0] : 0);
}

static void pre_glTexParameterIuiv(GLenum target, GLenum pname, const GLuint* params)
{
    check_lod_bias("glTexParameterIuiv", pname, params ? params[0] : 0);
}

static void pre_glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param)
{
    check_lod_bias("glSamplerParameterf", pname, param);
}

static void pre_glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat* param)
{
    check_lod_bias("glSamplerParameterfv", pname, param ? param[0] : 0);
}

static void pre_glSamplerParameteri(GLuint sampler, GLenum pname, GLint param)
{
    check_lod_bias("glSamplerParameteri", pname, param);
}

static void pre_glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint* param)
{
    check_lod_bias("glSamplerParameteriv", pname, param ? param[0] : 0);
}

static void pre_glSamplerParameterIiv(GLuint sampler, GLenum pname, const GLint* param)
{
    check_lod_bias("glSamplerParameterIiv", pname, param ? param[0] : 0);
}

static void pre_glSamplerParameterIuiv(GLuint sampler, GLenum pname, const GLuint* param)
{
    check_lod_bias("glSamplerParameterIuiv", pname, param ? param[0] : 0);
}

static void pre_glTextureParameterf(GLuint texture, GLenum pname, GLfloat param)
{
    check_lod_bias("glTextureParameterf", pname, param);
}

static void pre_glTextureParameterfv(GLuint texture, GLenum pname, const GLfloat* param)
{
    check_lod_bias("glTextureParameterfv", pname, param ? param[0] : 0);
}

static void pre_glTextureParameteri(GLuint texture, GLenum pname, GLint param)
{
    check_lod_bias("glTextureParameteri", pname, param);
}

static void pre_glTextureParameteriv(GLuint texture, GLenum pname, const GLint* param)
{
    check_lod_bias("glTextureParameteriv", pname, param ? param[0] : 0);
}

static void pre_glTextureParameterIiv(GLuint texture, GLenum pname, const GLint* params)
{
    check_lod_bias("glTextureParameterIiv", pname, params ? params[0] : 0);
}

static void pre_glTextureParameterIuiv(GLuint texture, GLenum pname, const GLuint* params)
{
    check_lod_bias("glTextureParameterIuiv", pname, params ? params[0] : 0);
}

static int buffer_texel_size(GLenum internalformat)
{
    switch (internalformat)
    {
    case GL_R8: case GL_R8I: case GL_R8UI:
        return 1;
    case GL_R16: case GL_R16F: case GL_R16I: case GL_R16UI: case GL_RG8: case GL_RG8I: case GL_RG8UI:
        return 2;
    case GL_R32F: case GL_R32I: case GL_R32UI: case GL_RG16: case GL_RG16F: case GL_RG16I: case GL_RG16UI:
    case GL_RGBA8: case GL_RGBA8I: case GL_RGBA8UI:
        return 4;
    case GL_RG32F: case GL_RG32I: case GL_RG32UI: case GL_RGBA16: case GL_RGBA16F: case GL_RGBA16I:
    case GL_RGBA16UI:
        return 8;
    case GL_RGB32F: case GL_RGB32I: case GL_RGB32UI:
        return 12;
    case GL_RGBA32F: case GL_RGBA32I: case GL_RGBA32UI:
        return 16;
    default:
        return 0;
    }
}

static void check_buffer_texture_range(const char* function, GLenum internalformat, GLsizeiptr size)
{
    const int texel_size = buffer_texel_size(internalformat);
    if (texel_size)
        check_at_most(function, "size / texel size", size / texel_size, GL_MAX_TEXTURE_BUFFER_SIZE);
}

static void pre_glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset,
                                 GLsizeiptr size)
{
    check_buffer_texture_range("glTexBufferRange", internalformat, size);
}

static void pre_glTextureBufferRange(GLuint texture, GLenum internalformat, GLuint buffer, GLintptr offset,
                                     GLsizeiptr size)
{
    check_buffer_texture_range("glTextureBufferRange", internalformat, size);
}

static int attribute_slots(GLenum type)
{
    switch (type)
    {
    case GL_FLOAT_MAT2: case GL_FLOAT_MAT2x3: case GL_FLOAT_MAT2x4:
        return 2;
    case GL_FLOAT_MAT3: case GL_FLOAT_MAT3x2: case GL_FLOAT_MAT3x4:
        return 3;
    case GL_FLOAT_MAT4: case GL_FLOAT_MAT4x2: case GL_FLOAT_MAT4x3:
        return 4;
    default:
        return 1;
    }
}

static int uniform_components(GLenum type)
{
    switch (type)
    {
    case GL_FLOAT: case GL_INT: case GL_UNSIGNED_INT: case GL_BOOL:
        return 1;
    case GL_FLOAT_VEC2: case GL_INT_VEC2: case GL_UNSIGNED_INT_VEC2: case GL_BOOL_VEC2:
        return 2;
    case GL_FLOAT_VEC3: case GL_INT_VEC3: case GL_UNSIGNED_INT_VEC3: case GL_BOOL_VEC3:
        return 3;
    case GL_FLOAT_VEC4: case GL_INT_VEC4: case GL_UNSIGNED_INT_VEC4: case GL_BOOL_VEC4: case GL_FLOAT_MAT2:
        return 4;
    case GL_FLOAT_MAT2x3: case GL_FLOAT_MAT3x2:
        return 6;
    case GL_FLOAT_MAT2x4: case GL_FLOAT_MAT4x2:
        return 8;
    case GL_FLOAT_MAT3:
        return 9;
    case GL_FLOAT_MAT3x4: case GL_FLOAT_MAT4x3:
        return 12;
    case GL_FLOAT_MAT4:
        return 16;
    default:
        return 0;
    }
}

static bool is_sampler_type(GLenum type)
{
    switch (type)
    {
    case GL_SAMPLER_1D: case GL_SAMPLER_2D: case GL_SAMPLER_3D: case GL_SAMPLER_CUBE:
    case GL_SAMPLER_1D_SHADOW: case GL_SAMPLER_2D_SHADOW: case GL_SAMPLER_1D_ARRAY: case GL_SAMPLER_2D_ARRAY:
    case GL_SAMPLER_1D_ARRAY_SHADOW: case GL_SAMPLER_2D_ARRAY_SHADOW: case GL_SAMPLER_2D_MULTISAMPLE:
    case GL_SAMPLER_2D_MULTISAMPLE_ARRAY: case GL_SAMPLER_CUBE_SHADOW: case GL_SAMPLER_BUFFER:
    case GL_SAMPLER_2D_RECT: case GL_SAMPLER_2D_RECT_SHADOW: case GL_SAMPLER_CUBE_MAP_ARRAY:
    case GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:
    case GL_INT_SAMPLER_1D: case GL_INT_SAMPLER_2D: case GL_INT_SAMPLER_3D: case GL_INT_SAMPLER_CUBE:
    case GL_INT_SAMPLER_1D_ARRAY: case GL_INT_SAMPLER_2D_ARRAY: case GL_INT_SAMPLER_2D_MULTISAMPLE:
    case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY: case GL_INT_SAMPLER_BUFFER: case GL_INT_SAMPLER_2D_RECT:
    case GL_INT_SAMPLER_CUBE_MAP_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_1D: case GL_UNSIGNED_INT_SAMPLER_2D: case GL_UNSIGNED_INT_SAMPLER_3D:
    case GL_UNSIGNED_INT_SAMPLER_CUBE: case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY: case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY: case GL_UNSIGNED_INT_SAMPLER_BUFFER:
    case GL_UNSIGNED_INT_SAMPLER_2D_RECT: case GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY:
        return true;
    default:
        return false;
    }
}

struct sampler_location
{
    GLint location, first_element, element_count;
    char* name;
};

struct program_samplers
{
    GLuint program;
    struct sampler_location* locations;
    size_t count;
};

static struct program_samplers* program_samplers;
static size_t program_samplers_count;

static struct program_samplers* find_program_samplers(GLuint program)
{
    for (size_t i = 0; i < program_samplers_count; i++)
        if (program_samplers[i].program == program)
            return &program_samplers[i];
    return NULL;
}

static void drop_program_samplers(GLuint program)
{
    struct program_samplers* p = find_program_samplers(program);
    if (p)
    {
        for (size_t i = 0; i < p->count; i++)
            free(p->locations[i].name);
        free(p->locations);
        *p = program_samplers[--program_samplers_count];
    }
}

static void add_sampler_location(struct program_samplers* p, struct sampler_location location, const char* name)
{
    p->locations = realloc(p->locations, (p->count + 1) * sizeof *p->locations);
    location.name = _strdup(name);
    if (!p->locations || !location.name)
        fail("out of memory recording the sampler %s", name);
    p->locations[p->count++] = location;
}

static const struct
{
    const char* name;
    GLenum referenced_by, block_referenced_by, max_components, max_units, max_blocks;
} shader_stages[] = {
    { "vertex", GL_REFERENCED_BY_VERTEX_SHADER, GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER,
      GL_MAX_VERTEX_UNIFORM_COMPONENTS, GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, GL_MAX_VERTEX_UNIFORM_BLOCKS },
    { "geometry", GL_REFERENCED_BY_GEOMETRY_SHADER, GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER,
      0, GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS, GL_MAX_GEOMETRY_UNIFORM_BLOCKS },
    { "fragment", GL_REFERENCED_BY_FRAGMENT_SHADER, GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER,
      GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, GL_MAX_TEXTURE_IMAGE_UNITS, GL_MAX_FRAGMENT_UNIFORM_BLOCKS },
};

enum { shader_stage_count = sizeof shader_stages / sizeof *shader_stages };

static void check_linked_count(GLuint program, const char* what, long long count, GLenum pname)
{
    const struct minimum_value* m = find_minimum_value(pname);
    if (count > m->value)
        abort_on_limit_violation("glLinkProgram linked program %u with %lld %s, over %s = %g, the GL 3.3 minimum",
                                 program, count, what, m->name, m->value);
}

static void check_linked_attributes(GLuint program)
{
    const struct minimum_value* m = find_minimum_value(GL_MAX_VERTEX_ATTRIBS);
    GLint count = 0;
    char name[256];
    mesa_queries.GetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &count);
    for (GLint i = 0; i < count; i++)
    {
        GLint size = 0;
        GLenum type = 0;
        mesa_queries.GetActiveAttrib(program, (GLuint)i, sizeof name, NULL, &size, &type, name);
        const GLint location = mesa_queries.GetAttribLocation(program, name);
        const GLint last = location + size * attribute_slots(type) - 1;
        if (location >= 0 && last >= m->value)
            abort_on_limit_violation("glLinkProgram linked program %u with attribute %s at locations %d to %d, "
                                     "not below %s = %g, the GL 3.3 minimum", program, name, location, last,
                                     m->name, m->value);
    }
}

static void check_linked_uniform_blocks(GLuint program)
{
    const struct minimum_value* max_size = find_minimum_value(GL_MAX_UNIFORM_BLOCK_SIZE);
    long long stage_blocks[shader_stage_count] = { 0 }, blocks = 0;
    GLint count = 0;
    char name[256];
    mesa_queries.GetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &count);
    for (GLint i = 0; i < count; i++)
    {
        GLint size = 0;
        mesa_queries.GetActiveUniformBlockiv(program, (GLuint)i, GL_UNIFORM_BLOCK_DATA_SIZE, &size);
        if (size > max_size->value)
        {
            mesa_queries.GetActiveUniformBlockName(program, (GLuint)i, sizeof name, NULL, name);
            abort_on_limit_violation("glLinkProgram linked program %u with uniform block %s of %d bytes, over "
                                     "%s = %g, the GL 3.3 minimum", program, name, size, max_size->name,
                                     max_size->value);
        }
        for (int s = 0; s < shader_stage_count; s++)
        {
            GLint is_referenced = 0;
            mesa_queries.GetActiveUniformBlockiv(program, (GLuint)i, shader_stages[s].block_referenced_by,
                                                 &is_referenced);
            stage_blocks[s] += is_referenced != 0;
            blocks += is_referenced != 0;
        }
    }
    for (int s = 0; s < shader_stage_count; s++)
    {
        snprintf(name, sizeof name, "%s shader uniform blocks", shader_stages[s].name);
        check_linked_count(program, name, stage_blocks[s], shader_stages[s].max_blocks);
    }
    check_linked_count(program, "uniform blocks summed over shaders", blocks, GL_MAX_COMBINED_UNIFORM_BLOCKS);
}

static void check_linked_uniforms(GLuint program)
{
    static const GLenum props[] = { GL_TYPE, GL_ARRAY_SIZE, GL_BLOCK_INDEX, GL_LOCATION,
                                    GL_REFERENCED_BY_VERTEX_SHADER, GL_REFERENCED_BY_GEOMETRY_SHADER,
                                    GL_REFERENCED_BY_FRAGMENT_SHADER };
    long long stage_components[shader_stage_count] = { 0 }, stage_samplers[shader_stage_count] = { 0 };
    long long samplers = 0;
    GLint count = 0;
    char name[256], element[300];

    drop_program_samplers(program);
    program_samplers = realloc(program_samplers, (program_samplers_count + 1) * sizeof *program_samplers);
    if (!program_samplers)
        fail("%s", "out of memory recording the samplers of a program");
    struct program_samplers* p = &program_samplers[program_samplers_count++];
    *p = (struct program_samplers){ program, NULL, 0 };

    mesa_queries.GetProgramInterfaceiv(program, GL_UNIFORM, GL_ACTIVE_RESOURCES, &count);
    for (GLint i = 0; i < count; i++)
    {
        GLint v[sizeof props / sizeof *props] = { 0 };
        mesa_queries.GetProgramResourceiv(program, GL_UNIFORM, (GLuint)i, sizeof props / sizeof *props, props,
                                          sizeof v / sizeof *v, NULL, v);
        const GLenum type = (GLenum)v[0];
        const GLint array_size = v[1], block_index = v[2], location = v[3];
        if (block_index == -1)
        {
            for (int s = 0; s < shader_stage_count; s++)
                if (v[4 + s])
                {
                    stage_components[s] += (long long)uniform_components(type) * array_size;
                    stage_samplers[s] += is_sampler_type(type) ? array_size : 0;
                    samplers += is_sampler_type(type) ? array_size : 0;
                }
            if (is_sampler_type(type) && location >= 0)
            {
                mesa_queries.GetProgramResourceName(program, GL_UNIFORM, (GLuint)i, sizeof name, NULL, name);
                const size_t length = strlen(name);
                if (array_size > 1 && length > 3 && !strcmp(name + length - 3, "[0]"))
                    name[length - 3] = '\0';
                for (GLint e = 0; e < array_size; e++)
                {
                    snprintf(element, sizeof element, "%s[%d]", name, e);
                    const struct sampler_location record = {
                        e ? mesa_queries.GetUniformLocation(program, element) : location, e, array_size - e, NULL
                    };
                    if (record.location >= 0)
                        add_sampler_location(p, record, name);
                }
            }
        }
    }

    for (int s = 0; s < shader_stage_count; s++)
    {
        snprintf(name, sizeof name, "%s shader uniform components", shader_stages[s].name);
        if (shader_stages[s].max_components)
            check_linked_count(program, name, stage_components[s], shader_stages[s].max_components);
        snprintf(name, sizeof name, "%s shader samplers", shader_stages[s].name);
        check_linked_count(program, name, stage_samplers[s], shader_stages[s].max_units);
    }
    check_linked_count(program, "samplers summed over shaders", samplers, GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
}

static void post_glLinkProgram(GLuint program)
{
    GLint is_linked = 0;
    ensure_mesa_queries();
    mesa_queries.GetProgramiv(program, GL_LINK_STATUS, &is_linked);
    if (is_linked)
    {
        check_linked_attributes(program);
        check_linked_uniform_blocks(program);
        check_linked_uniforms(program);
    }
}

static void pre_glDeleteProgram(GLuint program)
{
    drop_program_samplers(program);
}

static void check_sampler_units(const char* function, GLuint program, GLint location, GLsizei count,
                                const GLint* values)
{
    const struct minimum_value* m = find_minimum_value(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
    const struct program_samplers* p = find_program_samplers(program);
    char element[300];
    for (size_t i = 0; p && values && i < p->count; i++)
    {
        const struct sampler_location* s = &p->locations[i];
        for (GLsizei j = 0; s->location == location && j < count && j < s->element_count; j++)
            if (values[j] >= m->value)
            {
                if (s->first_element || s->element_count > 1)
                    snprintf(element, sizeof element, "%s[%d]", s->name, s->first_element + j);
                else
                    snprintf(element, sizeof element, "%s", s->name);
                abort_on_limit_violation("%s passed texture unit %d for sampler %s at location %d of program %u, "
                                         "not below %s = %g, the GL 3.3 minimum", function, values[j], element,
                                         location, program, m->name, m->value);
            }
    }
}

static GLuint program_receiving_uniforms(void)
{
    GLint program = 0, pipeline = 0;
    ensure_mesa_queries();
    mesa_queries.GetIntegerv(GL_CURRENT_PROGRAM, &program);
    if (!program)
        mesa_queries.GetIntegerv(GL_PROGRAM_PIPELINE_BINDING, &pipeline);
    if (pipeline)
        mesa_queries.GetProgramPipelineiv((GLuint)pipeline, GL_ACTIVE_PROGRAM, &program);
    return (GLuint)program;
}

static void pre_glUniform1i(GLint location, GLint v0)
{
    if (program_samplers_count)
        check_sampler_units("glUniform1i", program_receiving_uniforms(), location, 1, &v0);
}

static void pre_glUniform1iv(GLint location, GLsizei count, const GLint* value)
{
    if (program_samplers_count)
        check_sampler_units("glUniform1iv", program_receiving_uniforms(), location, count, value);
}

static void pre_glProgramUniform1i(GLuint program, GLint location, GLint v0)
{
    check_sampler_units("glProgramUniform1i", program, location, 1, &v0);
}

static void pre_glProgramUniform1iv(GLuint program, GLint location, GLsizei count, const GLint* value)
{
    check_sampler_units("glProgramUniform1iv", program, location, count, value);
}

static void pre_glTransformFeedbackBufferBase(GLuint xfb, GLuint index, GLuint buffer)
{
    check_below("glTransformFeedbackBufferBase", "index", index, GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS);
}

static void pre_glTransformFeedbackBufferRange(GLuint xfb, GLuint index, GLuint buffer, GLintptr offset,
                                               GLsizeiptr size)
{
    check_below("glTransformFeedbackBufferRange", "index", index, GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS);
}

static void pre_glGetTransformFeedbacki_v(GLuint xfb, GLenum pname, GLuint index, GLint* param)
{
    check_indexed_state("glGetTransformFeedbacki_v", pname, index);
}

static void pre_glGetTransformFeedbacki64_v(GLuint xfb, GLenum pname, GLuint index, GLint64* param)
{
    check_indexed_state("glGetTransformFeedbacki64_v", pname, index);
}

static void pre_glBindVertexBuffers(GLuint first, GLsizei count, const GLuint* buffers, const GLintptr* offsets,
                                    const GLsizei* strides)
{
    check_at_most("glBindVertexBuffers", "first + count", (long long)first + count, GL_MAX_VERTEX_ATTRIB_BINDINGS);
}

static void pre_glVertexArrayVertexBuffers(GLuint vaobj, GLuint first, GLsizei count, const GLuint* buffers,
                                           const GLintptr* offsets, const GLsizei* strides)
{
    check_at_most("glVertexArrayVertexBuffers", "first + count", (long long)first + count,
                  GL_MAX_VERTEX_ATTRIB_BINDINGS);
}

static void check_framebuffer_parameter(const char* function, GLenum pname, GLint param)
{
    switch (pname)
    {
    case GL_FRAMEBUFFER_DEFAULT_WIDTH:
        check_at_most(function, "GL_FRAMEBUFFER_DEFAULT_WIDTH", param, GL_MAX_FRAMEBUFFER_WIDTH);
        break;
    case GL_FRAMEBUFFER_DEFAULT_HEIGHT:
        check_at_most(function, "GL_FRAMEBUFFER_DEFAULT_HEIGHT", param, GL_MAX_FRAMEBUFFER_HEIGHT);
        break;
    case GL_FRAMEBUFFER_DEFAULT_LAYERS:
        check_at_most(function, "GL_FRAMEBUFFER_DEFAULT_LAYERS", param, GL_MAX_FRAMEBUFFER_LAYERS);
        break;
    case GL_FRAMEBUFFER_DEFAULT_SAMPLES:
        check_at_most(function, "GL_FRAMEBUFFER_DEFAULT_SAMPLES", param, GL_MAX_FRAMEBUFFER_SAMPLES);
        break;
    default:
        break;
    }
}

static void pre_glFramebufferParameteri(GLenum target, GLenum pname, GLint param)
{
    check_framebuffer_parameter("glFramebufferParameteri", pname, param);
}

static void pre_glNamedFramebufferParameteri(GLuint framebuffer, GLenum pname, GLint param)
{
    check_framebuffer_parameter("glNamedFramebufferParameteri", pname, param);
}

static GLsizei debug_string_length(GLsizei length, const GLchar* string)
{
    return length >= 0 || !string ? length : (GLsizei)strlen(string);
}

static void pre_glDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                                     const GLchar* buf)
{
    check_below("glDebugMessageInsert", "message length", debug_string_length(length, buf),
                GL_MAX_DEBUG_MESSAGE_LENGTH);
}

static void pre_glPushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar* message)
{
    const struct minimum_value* m = find_minimum_value(GL_MAX_DEBUG_GROUP_STACK_DEPTH);
    GLint depth = 0;
    check_below("glPushDebugGroup", "message length", debug_string_length(length, message),
                GL_MAX_DEBUG_MESSAGE_LENGTH);
    ensure_mesa_queries();
    mesa_queries.GetIntegerv(GL_DEBUG_GROUP_STACK_DEPTH, &depth);
    if (depth >= m->value - 1)
        abort_on_limit_violation("glPushDebugGroup passed a group while the stack held %d, not below %s - 1 = %g, "
                                 "the %s minimum", depth, m->name, m->value - 1, m->spec);
}

static void pre_glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar* label)
{
    if (label)
        check_below("glObjectLabel", "label length", debug_string_length(length, label), GL_MAX_LABEL_LENGTH);
}

static void pre_glObjectPtrLabel(const void* ptr, GLsizei length, const GLchar* label)
{
    if (label)
        check_below("glObjectPtrLabel", "label length", debug_string_length(length, label), GL_MAX_LABEL_LENGTH);
}

#pragma GCC diagnostic pop
