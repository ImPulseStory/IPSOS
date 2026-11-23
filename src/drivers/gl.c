#include "../include/gl.h"
#include "../include/virtio_gpu.h"
#include "../include/gfx.h"

#define VIRGL_SURFACE_HANDLE 1u
#define VIRGL_DISPLAY_RESOURCE 1u
#define GL_DEPTH_RESOURCE 0x7fff0003u /* Private GL depth resource; resource 2 is the cursor. */
#define GL_DEPTH_SURFACE_HANDLE 2u
#define GL_MAX_BUFFERS 8u
#define GL_MAX_SHADERS 8u
#define GL_MAX_PROGRAMS 8u
#define GL_MAX_SHADER_SOURCE 4095u
#define GL_MAX_VERTEX_ATTRIBS_LOCAL VIRGL_MAX_VERTEX_ATTRIBS
#define GL_BUFFER_RESOURCE_BASE 0x1000u
#define GL_SHADER_HANDLE_BASE 0x2000u
#define GL_BLEND_HANDLE 0x3000u
#define GL_RASTERIZER_HANDLE 0x3001u
#define GL_DSA_HANDLE 0x3002u
#define GL_VERTEX_ELEMENTS_HANDLE 0x3003u
#define GL_SHADER_TOKEN_LIMIT 1024u

struct gl_buffer_slot {
    GLuint name;
    uint32_t resource;
    uint32_t size;
    int created;
    int attached;
};

struct gl_shader_slot {
    GLuint name;
    GLenum type;
    uint32_t handle;
    uint32_t source_length;
    char source[GL_MAX_SHADER_SOURCE + 1u];
    int compiled;
    int gpu_created;
};

struct gl_program_slot {
    GLuint name;
    GLuint vertex_shader;
    GLuint fragment_shader;
    int linked;
};

struct gl_vertex_attrib {
    GLuint buffer;
    uint32_t offset;
    uint32_t stride;
    GLint size;
    GLenum type;
    GLboolean normalized;
    int enabled;
};

static GLenum gl_error;
static int gl_ready;
static GLfloat clear_color[4];
static GLdouble clear_depth_value = 1.0;
static int display_resource_attached;
static int depth_resource_created;
static int depth_resource_attached;
static int color_surface_created;
static int depth_surface_created;
static int dsa_state_created;
static int depth_test_enabled;
static int depth_write_enabled = 1;
static uint32_t depth_function = 1u; /* PIPE_FUNC_LESS */
static GLuint next_buffer_name = 1u;
static GLuint next_shader_name = 1u;
static GLuint next_program_name = 1u;
static GLuint bound_array_buffer;
static GLuint bound_element_array_buffer;
static GLuint current_program;
static int vertex_elements_created;
static struct gl_buffer_slot gl_buffers[GL_MAX_BUFFERS];
static struct gl_shader_slot gl_shaders[GL_MAX_SHADERS];
static struct gl_program_slot gl_programs[GL_MAX_PROGRAMS];
static struct gl_vertex_attrib gl_attribs[GL_MAX_VERTEX_ATTRIBS_LOCAL];

static uint32_t float_bits(GLfloat value)
{
    union { GLfloat f; uint32_t u; } bits;
    bits.f = value;
    return bits.u;
}

static void gl_set_error(GLenum error)
{
    if (gl_error == GL_NO_ERROR)
        gl_error = error;
}

static struct gl_buffer_slot *gl_find_buffer(GLuint name)
{
    if (!name) return 0;
    for (uint32_t i = 0; i < GL_MAX_BUFFERS; ++i)
        if (gl_buffers[i].name == name) return &gl_buffers[i];
    return 0;
}

static struct gl_shader_slot *gl_find_shader(GLuint name)
{
    if (!name) return 0;
    for (uint32_t i = 0; i < GL_MAX_SHADERS; ++i)
        if (gl_shaders[i].name == name) return &gl_shaders[i];
    return 0;
}

static struct gl_program_slot *gl_find_program(GLuint name)
{
    if (!name) return 0;
    for (uint32_t i = 0; i < GL_MAX_PROGRAMS; ++i)
        if (gl_programs[i].name == name) return &gl_programs[i];
    return 0;
}

static int gl_submit_state(uint32_t object_type, const uint32_t *payload, uint32_t length)
{
    uint32_t commands[16];
    if (length + 1u > 16u) return -1;
    commands[0] = VIRGL_CMD0(VIRGL_CCMD_CREATE_OBJECT, object_type, length);
    for (uint32_t i = 0; i < length; ++i) commands[i + 1u] = payload[i];
    if (virgl_submit(VIRGL_CONTEXT_DEFAULT, commands, length + 1u) != 0)
        return -1;
    commands[0] = VIRGL_CMD0(VIRGL_CCMD_BIND_OBJECT, object_type, 1u);
    commands[1] = payload[0];
    return virgl_submit(VIRGL_CONTEXT_DEFAULT, commands, 2u);
}

static int gl_update_dsa_state(void)
{
    uint32_t commands[10];
    uint32_t cursor = 0u;
    uint32_t state = (depth_test_enabled ? (1u << 0) : 0u) |
                     (depth_write_enabled ? (1u << 1) : 0u) |
                     ((depth_function & 7u) << 2);

    if (dsa_state_created) {
        commands[cursor++] = VIRGL_CMD0(VIRGL_CCMD_DESTROY_OBJECT, VIRGL_OBJECT_DSA, 1u);
        commands[cursor++] = GL_DSA_HANDLE;
    }
    commands[cursor++] = VIRGL_CMD0(VIRGL_CCMD_CREATE_OBJECT, VIRGL_OBJECT_DSA, 5u);
    commands[cursor++] = GL_DSA_HANDLE;
    commands[cursor++] = state;
    commands[cursor++] = 0u;
    commands[cursor++] = 0u;
    commands[cursor++] = float_bits(0.0f);
    commands[cursor++] = VIRGL_CMD0(VIRGL_CCMD_BIND_OBJECT, VIRGL_OBJECT_DSA, 1u);
    commands[cursor++] = GL_DSA_HANDLE;

    if (virgl_submit(VIRGL_CONTEXT_DEFAULT, commands, cursor) != 0)
        return -1;
    dsa_state_created = 1;
    return 0;
}

static void gl_reset_context_bindings(void)
{
    gl_ready = 0;
    display_resource_attached = 0;
    depth_resource_attached = 0;
    color_surface_created = 0;
    depth_surface_created = 0;
    dsa_state_created = 0;
    vertex_elements_created = 0;
    current_program = 0u;

    for (uint32_t i = 0; i < GL_MAX_BUFFERS; ++i)
        gl_buffers[i].attached = 0;
    for (uint32_t i = 0; i < GL_MAX_SHADERS; ++i) {
        gl_shaders[i].compiled = 0;
        gl_shaders[i].gpu_created = 0;
    }
    for (uint32_t i = 0; i < GL_MAX_PROGRAMS; ++i)
        gl_programs[i].linked = 0;
}

static int gl_init_fixed_state(void)
{
    uint32_t blend[11];
    uint32_t rasterizer[9];

    blend[0] = GL_BLEND_HANDLE;
    blend[1] = 0u;
    blend[2] = 0u;
    for (uint32_t i = 0; i < 8u; ++i)
        blend[3u + i] = i == 0u ? (0x0fu << 27) : 0u;
    if (gl_submit_state(VIRGL_OBJECT_BLEND, blend, 11u) != 0)
        return -1;

    if (gl_update_dsa_state() != 0)
        return -1;

    rasterizer[0] = GL_RASTERIZER_HANDLE;
    rasterizer[1] = (1u << 1) | (1u << 29) | (1u << 30); /* depth clip, half pixel, bottom edge */
    rasterizer[2] = float_bits(1.0f);
    rasterizer[3] = 0u;
    rasterizer[4] = 0u;
    rasterizer[5] = float_bits(1.0f);
    rasterizer[6] = 0u;
    rasterizer[7] = 0u;
    rasterizer[8] = 0u;
    return gl_submit_state(VIRGL_OBJECT_RASTERIZER, rasterizer, 9u);
}

int glInit(void)
{
    uint32_t commands[6];
    uint32_t width;
    uint32_t height;

    if (gl_ready) return 0;
    if (virgl_init() != 0) { gl_set_error(GL_INVALID_OPERATION); return -1; }
    width = gfx_width();
    height = gfx_height();
    if (!width || !height) { gl_set_error(GL_INVALID_OPERATION); return -1; }

    if (!display_resource_attached) {
        if (virgl_resource_attach(VIRGL_CONTEXT_DEFAULT, VIRGL_DISPLAY_RESOURCE) != 0) {
            gl_set_error(GL_INVALID_OPERATION);
            return -1;
        }
        display_resource_attached = 1;
    }

    /* Resource 2 is reserved by virtio_gpu.c for the hardware cursor. */
    if (!depth_resource_created) {
        if (virgl_resource_create_3d(GL_DEPTH_RESOURCE, VIRGL_PIPE_TEXTURE_2D,
                                     VIRGL_FORMAT_Z32_FLOAT, VIRGL_BIND_DEPTH_STENCIL,
                                     width, height, 1u) != 0) {
            gl_set_error(GL_INVALID_OPERATION);
            return -1;
        }
        depth_resource_created = 1;
    }
    if (!depth_resource_attached) {
        if (virgl_resource_attach(VIRGL_CONTEXT_DEFAULT, GL_DEPTH_RESOURCE) != 0) {
            gl_set_error(GL_INVALID_OPERATION);
            return -1;
        }
        depth_resource_attached = 1;
    }

    /* Buffer resources survive a context reset; attach them to the new context. */
    for (uint32_t i = 0; i < GL_MAX_BUFFERS; ++i) {
        if (gl_buffers[i].name && gl_buffers[i].created && !gl_buffers[i].attached) {
            if (virgl_resource_attach(VIRGL_CONTEXT_DEFAULT, gl_buffers[i].resource) != 0) {
                gl_set_error(GL_INVALID_OPERATION);
                return -1;
            }
            gl_buffers[i].attached = 1;
        }
    }

    if (!color_surface_created) {
        commands[0] = VIRGL_CMD0(VIRGL_CCMD_CREATE_OBJECT, VIRGL_OBJECT_SURFACE, 5u);
        commands[1] = VIRGL_SURFACE_HANDLE;
        commands[2] = VIRGL_DISPLAY_RESOURCE;
        commands[3] = VIRGL_FORMAT_B8G8R8X8_UNORM;
        commands[4] = 0u;
        commands[5] = 0u;
        if (virgl_submit(VIRGL_CONTEXT_DEFAULT, commands, 6u) != 0) {
            gl_set_error(GL_INVALID_OPERATION);
            return -1;
        }
        color_surface_created = 1;
    }
    if (!depth_surface_created) {
        commands[0] = VIRGL_CMD0(VIRGL_CCMD_CREATE_OBJECT, VIRGL_OBJECT_SURFACE, 5u);
        commands[1] = GL_DEPTH_SURFACE_HANDLE;
        commands[2] = GL_DEPTH_RESOURCE;
        commands[3] = VIRGL_FORMAT_Z32_FLOAT;
        commands[4] = 0u;
        commands[5] = 0u;
        if (virgl_submit(VIRGL_CONTEXT_DEFAULT, commands, 6u) != 0) {
            gl_set_error(GL_INVALID_OPERATION);
            return -1;
        }
        depth_surface_created = 1;
    }

    commands[0] = VIRGL_CMD0(VIRGL_CCMD_SET_FRAMEBUFFER_STATE, 0u, 3u);
    commands[1] = 1u; /* one color attachment */
    commands[2] = GL_DEPTH_SURFACE_HANDLE;
    commands[3] = VIRGL_SURFACE_HANDLE;
    if (virgl_submit(VIRGL_CONTEXT_DEFAULT, commands, 4u) != 0) {
        gl_set_error(GL_INVALID_OPERATION);
        return -1;
    }

    /* Restore OpenGL defaults before creating the bound DSA object. This matters
       after destroying and recreating the VirGL context. */
    clear_color[0] = 0.0f; clear_color[1] = 0.0f;
    clear_color[2] = 0.0f; clear_color[3] = 1.0f;
    clear_depth_value = 1.0;
    depth_test_enabled = 0;
    depth_write_enabled = 1;
    depth_function = 1u; /* PIPE_FUNC_LESS */
    if (gl_init_fixed_state() != 0) {
        gl_set_error(GL_INVALID_OPERATION);
        return -1;
    }
    gl_ready = 1;
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    return 0;
}

int glIsReady(void)
{
    return gl_ready;
}

static GLfloat gl_clamp_color(GLfloat value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    clear_color[0] = gl_clamp_color(red);
    clear_color[1] = gl_clamp_color(green);
    clear_color[2] = gl_clamp_color(blue);
    clear_color[3] = gl_clamp_color(alpha);
}

void glClear(GLbitfield mask)
{
    uint32_t commands[9];
    union { double d; uint32_t u[2]; } depth;
    uint32_t clear_flags = 0u;

    if (mask == 0u) return;
    if (!gl_ready || (mask & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT))) {
        gl_set_error(GL_INVALID_OPERATION);
        return;
    }
    if (mask & GL_COLOR_BUFFER_BIT) clear_flags |= VIRGL_PIPE_CLEAR_COLOR0;
    /* OpenGL depth write mask also affects depth clears. */
    if ((mask & GL_DEPTH_BUFFER_BIT) && depth_write_enabled)
        clear_flags |= VIRGL_PIPE_CLEAR_DEPTH;
    if (!clear_flags) return;

    depth.d = clear_depth_value;
    commands[0] = VIRGL_CMD0(VIRGL_CCMD_CLEAR, 0u, 8u);
    commands[1] = clear_flags;
    commands[2] = float_bits(clear_color[0]);
    commands[3] = float_bits(clear_color[1]);
    commands[4] = float_bits(clear_color[2]);
    commands[5] = float_bits(clear_color[3]);
    commands[6] = depth.u[0];
    commands[7] = depth.u[1];
    commands[8] = 0u;
    if (virgl_submit(VIRGL_CONTEXT_DEFAULT, commands, 9u) != 0)
        gl_set_error(GL_INVALID_OPERATION);
}

void glClearDepth(GLdouble depth)
{
    if (depth != depth) { /* reject NaN */
        gl_set_error(GL_INVALID_VALUE);
        return;
    }
    if (depth < 0.0) depth = 0.0;
    if (depth > 1.0) depth = 1.0;
    clear_depth_value = depth;
}

static int gl_map_depth_function(GLenum function, uint32_t *pipe_function)
{
    switch (function) {
        case GL_NEVER: *pipe_function = 0u; return 0;
        case GL_LESS: *pipe_function = 1u; return 0;
        case GL_EQUAL: *pipe_function = 2u; return 0;
        case GL_LEQUAL: *pipe_function = 3u; return 0;
        case GL_GREATER: *pipe_function = 4u; return 0;
        case GL_NOTEQUAL: *pipe_function = 5u; return 0;
        case GL_GEQUAL: *pipe_function = 6u; return 0;
        case GL_ALWAYS: *pipe_function = 7u; return 0;
        default: return -1;
    }
}

void glEnable(GLenum capability)
{
    if (capability != GL_DEPTH_TEST) { gl_set_error(GL_INVALID_ENUM); return; }
    if (!gl_ready) { gl_set_error(GL_INVALID_OPERATION); return; }
    if (!depth_test_enabled) {
        depth_test_enabled = 1;
        if (gl_update_dsa_state() != 0) gl_set_error(GL_INVALID_OPERATION);
    }
}

void glDisable(GLenum capability)
{
    if (capability != GL_DEPTH_TEST) { gl_set_error(GL_INVALID_ENUM); return; }
    if (!gl_ready) { gl_set_error(GL_INVALID_OPERATION); return; }
    if (depth_test_enabled) {
        depth_test_enabled = 0;
        if (gl_update_dsa_state() != 0) gl_set_error(GL_INVALID_OPERATION);
    }
}

void glDepthFunc(GLenum function)
{
    uint32_t mapped;
    if (gl_map_depth_function(function, &mapped) != 0) { gl_set_error(GL_INVALID_ENUM); return; }
    if (!gl_ready) { gl_set_error(GL_INVALID_OPERATION); return; }
    depth_function = mapped;
    if (gl_update_dsa_state() != 0) gl_set_error(GL_INVALID_OPERATION);
}

void glDepthMask(GLboolean enabled)
{
    if (!gl_ready) { gl_set_error(GL_INVALID_OPERATION); return; }
    depth_write_enabled = enabled != GL_FALSE;
    if (gl_update_dsa_state() != 0) gl_set_error(GL_INVALID_OPERATION);
}

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    uint32_t commands[8];
    GLfloat sx, sy, tx, ty;
    if (!gl_ready || width <= 0 || height <= 0 || x < 0 || y < 0 ||
        (uint32_t)x >= gfx_width() || (uint32_t)y >= gfx_height()) {
        gl_set_error(GL_INVALID_VALUE);
        return;
    }
    if ((uint32_t)width > gfx_width() - (uint32_t)x) width = (GLsizei)(gfx_width() - (uint32_t)x);
    if ((uint32_t)height > gfx_height() - (uint32_t)y) height = (GLsizei)(gfx_height() - (uint32_t)y);
    sx = (GLfloat)width * 0.5f; sy = (GLfloat)height * 0.5f;
    tx = (GLfloat)x + sx; ty = (GLfloat)y + sy;
    commands[0] = VIRGL_CMD0(VIRGL_CCMD_SET_VIEWPORT_STATE, 0u, 7u);
    commands[1] = 0u;
    commands[2] = float_bits(sx);
    commands[3] = float_bits(sy);
    commands[4] = float_bits(0.5f);
    commands[5] = float_bits(tx);
    commands[6] = float_bits(ty);
    commands[7] = float_bits(0.5f);
    if (virgl_submit(VIRGL_CONTEXT_DEFAULT, commands, 8u) != 0) gl_set_error(GL_INVALID_OPERATION);
}

void glFlush(void)
{
    if (!gl_ready || virtio_gpu_flush_rect(0u, 0u, gfx_width(), gfx_height()) != 0)
        gl_set_error(GL_INVALID_OPERATION);
}

GLenum glGetError(void)
{
    GLenum result = gl_error;
    gl_error = GL_NO_ERROR;
    return result;
}

int glCreateContext(uint32_t context_id, const char *name)
{
    int result = virgl_context_create(context_id, name);
    if (result != 0) gl_set_error(GL_INVALID_OPERATION);
    return result;
}

int glDestroyContext(uint32_t context_id)
{
    int result = virgl_context_destroy(context_id);
    if (result != 0) gl_set_error(GL_INVALID_OPERATION);
    else gl_reset_context_bindings();
    return result;
}

int glCreateResource3D(uint32_t resource_id, uint32_t target, uint32_t format, uint32_t bind, uint32_t width, uint32_t height, uint32_t depth)
{
    int result = virgl_resource_create_3d(resource_id, target, format, bind, width, height, depth);
    if (result != 0) gl_set_error(GL_INVALID_OPERATION);
    return result;
}

int glAttachResource(uint32_t context_id, uint32_t resource_id)
{
    int result = virgl_resource_attach(context_id, resource_id);
    if (result != 0) gl_set_error(GL_INVALID_OPERATION);
    return result;
}

int glSubmitCommands(uint32_t context_id, const uint32_t *dwords, uint32_t count)
{
    int result = virgl_submit(context_id, dwords, count);
    if (result != 0) gl_set_error(GL_INVALID_OPERATION);
    return result;
}

void glGenBuffers(GLsizei count, GLuint *buffers)
{
    if (!gl_ready || count < 0 || (!buffers && count)) { gl_set_error(GL_INVALID_VALUE); return; }
    for (GLsizei n = 0; n < count; ++n) {
        struct gl_buffer_slot *slot = 0;
        for (uint32_t i = 0; i < GL_MAX_BUFFERS; ++i)
            if (!gl_buffers[i].name) { slot = &gl_buffers[i]; break; }
        if (!slot) { buffers[n] = 0u; gl_set_error(GL_OUT_OF_MEMORY); continue; }
        slot->name = next_buffer_name++;
        slot->resource = GL_BUFFER_RESOURCE_BASE + (uint32_t)(slot - gl_buffers);
        buffers[n] = slot->name;
    }
}

void glBindBuffer(GLenum target, GLuint buffer)
{
    if (target != GL_ARRAY_BUFFER && target != GL_ELEMENT_ARRAY_BUFFER) {
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
    if (buffer && !gl_find_buffer(buffer)) {
        gl_set_error(GL_INVALID_VALUE);
        return;
    }
    if (target == GL_ARRAY_BUFFER)
        bound_array_buffer = buffer;
    else
        bound_element_array_buffer = buffer;
}

void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
    struct gl_buffer_slot *slot;
    (void)usage;
    GLuint bound;
    if (target != GL_ARRAY_BUFFER && target != GL_ELEMENT_ARRAY_BUFFER) {
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
    bound = target == GL_ARRAY_BUFFER ? bound_array_buffer : bound_element_array_buffer;
    if (size <= 0 || !bound) { gl_set_error(GL_INVALID_VALUE); return; }
    slot = gl_find_buffer(bound);
    if (!slot) { gl_set_error(GL_INVALID_OPERATION); return; }
    if (!slot->created) {
        if (virgl_resource_create_3d(slot->resource, VIRGL_PIPE_BUFFER, VIRGL_FORMAT_R8_UNORM,
                                    VIRGL_BIND_VERTEX_BUFFER | VIRGL_BIND_INDEX_BUFFER,
                                    (uint32_t)size, 1u, 1u) != 0) {
            gl_set_error(GL_INVALID_OPERATION);
            return;
        }
        slot->created = 1;
        slot->size = (uint32_t)size;
    } else if (slot->size != (uint32_t)size) {
        gl_set_error(GL_INVALID_OPERATION); /* resource resizing needs RESOURCE_UNREF support */
        return;
    }
    if (!slot->attached) {
        if (virgl_resource_attach(VIRGL_CONTEXT_DEFAULT, slot->resource) != 0) {
            gl_set_error(GL_INVALID_OPERATION);
            return;
        }
        slot->attached = 1;
    }
    if (data && virgl_resource_inline_write(slot->resource, 0u, data, (uint32_t)size) != 0)
        gl_set_error(GL_INVALID_OPERATION);
}

void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data)
{
    struct gl_buffer_slot *slot;
    GLuint bound;
    if (target != GL_ARRAY_BUFFER && target != GL_ELEMENT_ARRAY_BUFFER) {
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
    bound = target == GL_ARRAY_BUFFER ? bound_array_buffer : bound_element_array_buffer;
    slot = gl_find_buffer(bound);
    if (!slot || !slot->created || offset < 0 || size < 0 || !data ||
        (uint32_t)offset > slot->size || (uint32_t)size > slot->size - (uint32_t)offset) {
        gl_set_error(GL_INVALID_VALUE);
        return;
    }
    if (size && virgl_resource_inline_write(slot->resource, (uint32_t)offset, data, (uint32_t)size) != 0)
        gl_set_error(GL_INVALID_OPERATION);
}

void glEnableVertexAttribArray(GLuint index)
{
    if (index >= GL_MAX_VERTEX_ATTRIBS_LOCAL) { gl_set_error(GL_INVALID_VALUE); return; }
    gl_attribs[index].enabled = 1;
}

void glDisableVertexAttribArray(GLuint index)
{
    if (index >= GL_MAX_VERTEX_ATTRIBS_LOCAL) { gl_set_error(GL_INVALID_VALUE); return; }
    gl_attribs[index].enabled = 0;
}

void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer)
{
    struct gl_vertex_attrib *attrib;
    if (index >= GL_MAX_VERTEX_ATTRIBS_LOCAL || size < 1 || size > 4 || stride < 0) {
        gl_set_error(GL_INVALID_VALUE); return;
    }
    if (type != GL_FLOAT || normalized) { gl_set_error(GL_INVALID_ENUM); return; }
    if (!bound_array_buffer || !gl_find_buffer(bound_array_buffer)) { gl_set_error(GL_INVALID_OPERATION); return; }
    attrib = &gl_attribs[index];
    attrib->buffer = bound_array_buffer;
    attrib->size = size;
    attrib->type = type;
    attrib->normalized = normalized;
    attrib->stride = (uint32_t)stride;
    attrib->offset = (uint32_t)(uintptr_t)pointer;
}

GLuint glCreateShader(GLenum type)
{
    if (!gl_ready || (type != GL_VERTEX_SHADER && type != GL_FRAGMENT_SHADER)) {
        gl_set_error(GL_INVALID_ENUM); return 0u;
    }
    for (uint32_t i = 0; i < GL_MAX_SHADERS; ++i) {
        if (!gl_shaders[i].name) {
            gl_shaders[i].name = next_shader_name++;
            gl_shaders[i].type = type;
            gl_shaders[i].handle = GL_SHADER_HANDLE_BASE + i;
            gl_shaders[i].source_length = 0u;
            gl_shaders[i].compiled = 0;
            gl_shaders[i].gpu_created = 0;
            return gl_shaders[i].name;
        }
    }
    gl_set_error(GL_OUT_OF_MEMORY);
    return 0u;
}

int glShaderSourceTGSI(GLuint shader, const char *source)
{
    struct gl_shader_slot *slot = gl_find_shader(shader);
    uint32_t length = 0;
    if (!slot || !source) { gl_set_error(GL_INVALID_VALUE); return -1; }
    while (source[length] && length <= GL_MAX_SHADER_SOURCE) ++length;
    if (length == 0u || length > GL_MAX_SHADER_SOURCE) { gl_set_error(GL_INVALID_VALUE); return -1; }
    for (uint32_t i = 0; i < length; ++i) slot->source[i] = source[i];
    slot->source[length] = '\0';
    slot->source_length = length + 1u;
    slot->compiled = 0;
    for (uint32_t i = 0; i < GL_MAX_PROGRAMS; ++i) {
        if (gl_programs[i].name &&
            (gl_programs[i].vertex_shader == shader || gl_programs[i].fragment_shader == shader))
            gl_programs[i].linked = 0;
    }
    return 0;
}

int glCompileShader(GLuint shader)
{
    struct gl_shader_slot *slot = gl_find_shader(shader);
    uint32_t commands[6u + ((GL_MAX_SHADER_SOURCE + 1u + 3u) / 4u)];
    uint32_t source_dwords, total;
    uint8_t *dst;
    if (!slot || !slot->source_length) { gl_set_error(GL_INVALID_OPERATION); return -1; }
    source_dwords = (slot->source_length + 3u) / 4u;
    total = 6u + source_dwords;
    if (total > VIRGL_MAX_SUBMIT_DWORDS) { gl_set_error(GL_INVALID_VALUE); return -1; }
    if (slot->gpu_created) {
        uint32_t destroy[2];
        destroy[0] = VIRGL_CMD0(VIRGL_CCMD_DESTROY_OBJECT, VIRGL_OBJECT_SHADER, 1u);
        destroy[1] = slot->handle;
        if (virgl_submit(VIRGL_CONTEXT_DEFAULT, destroy, 2u) != 0) {
            gl_set_error(GL_INVALID_OPERATION);
            return -1;
        }
        slot->gpu_created = 0;
    }
    commands[0] = VIRGL_CMD0(VIRGL_CCMD_CREATE_OBJECT, VIRGL_OBJECT_SHADER, 5u + source_dwords);
    commands[1] = slot->handle;
    commands[2] = slot->type == GL_VERTEX_SHADER ? VIRGL_PIPE_SHADER_VERTEX : VIRGL_PIPE_SHADER_FRAGMENT;
    commands[3] = slot->source_length;
    commands[4] = GL_SHADER_TOKEN_LIMIT;
    commands[5] = 0u;
    dst = (uint8_t *)&commands[6];
    for (uint32_t i = 0; i < source_dwords * 4u; ++i)
        dst[i] = i < slot->source_length ? (uint8_t)slot->source[i] : 0u;
    if (virgl_submit(VIRGL_CONTEXT_DEFAULT, commands, total) != 0) {
        gl_set_error(GL_INVALID_OPERATION);
        return -1;
    }
    slot->compiled = 1;
    slot->gpu_created = 1;
    return 0;
}

GLuint glCreateProgram(void)
{
    if (!gl_ready) { gl_set_error(GL_INVALID_OPERATION); return 0u; }
    for (uint32_t i = 0; i < GL_MAX_PROGRAMS; ++i) {
        if (!gl_programs[i].name) {
            gl_programs[i].name = next_program_name++;
            gl_programs[i].vertex_shader = 0u;
            gl_programs[i].fragment_shader = 0u;
            gl_programs[i].linked = 0;
            return gl_programs[i].name;
        }
    }
    gl_set_error(GL_OUT_OF_MEMORY);
    return 0u;
}

int glAttachShader(GLuint program, GLuint shader)
{
    struct gl_program_slot *p = gl_find_program(program);
    struct gl_shader_slot *s = gl_find_shader(shader);
    if (!p || !s) { gl_set_error(GL_INVALID_VALUE); return -1; }
    if (s->type == GL_VERTEX_SHADER) p->vertex_shader = shader;
    else if (s->type == GL_FRAGMENT_SHADER) p->fragment_shader = shader;
    else { gl_set_error(GL_INVALID_OPERATION); return -1; }
    p->linked = 0;
    return 0;
}

int glLinkProgram(GLuint program)
{
    struct gl_program_slot *p = gl_find_program(program);
    struct gl_shader_slot *vs;
    struct gl_shader_slot *fs;
    if (!p || !p->vertex_shader || !p->fragment_shader) { gl_set_error(GL_INVALID_OPERATION); return -1; }
    vs = gl_find_shader(p->vertex_shader);
    fs = gl_find_shader(p->fragment_shader);
    if (!vs || !fs || !vs->compiled || !fs->compiled) { gl_set_error(GL_INVALID_OPERATION); return -1; }
    /* Program linking is a guest-side pairing of TGSI stages; VirGL binds stages separately. */
    p->linked = 1;
    return 0;
}

void glUseProgram(GLuint program)
{
    struct gl_program_slot *p;
    struct gl_shader_slot *vs;
    struct gl_shader_slot *fs;
    uint32_t commands[6];
    if (!program) { gl_set_error(GL_INVALID_OPERATION); return; }
    p = gl_find_program(program);
    if (!p || !p->linked) { gl_set_error(GL_INVALID_OPERATION); return; }
    vs = gl_find_shader(p->vertex_shader);
    fs = gl_find_shader(p->fragment_shader);
    if (!vs || !fs) { gl_set_error(GL_INVALID_OPERATION); return; }
    commands[0] = VIRGL_CMD0(VIRGL_CCMD_BIND_SHADER, 0u, 2u);
    commands[1] = vs->handle;
    commands[2] = VIRGL_PIPE_SHADER_VERTEX;
    commands[3] = VIRGL_CMD0(VIRGL_CCMD_BIND_SHADER, 0u, 2u);
    commands[4] = fs->handle;
    commands[5] = VIRGL_PIPE_SHADER_FRAGMENT;
    if (virgl_submit(VIRGL_CONTEXT_DEFAULT, commands, 6u) != 0) {
        gl_set_error(GL_INVALID_OPERATION); return;
    }
    current_program = program;
}

static uint32_t gl_primitive_to_virgl(GLenum mode)
{
    switch (mode) {
        case GL_POINTS: return VIRGL_PIPE_PRIM_POINTS;
        case GL_LINES: return VIRGL_PIPE_PRIM_LINES;
        case GL_LINE_LOOP: return VIRGL_PIPE_PRIM_LINE_LOOP;
        case GL_LINE_STRIP: return VIRGL_PIPE_PRIM_LINE_STRIP;
        case GL_TRIANGLES: return VIRGL_PIPE_PRIM_TRIANGLES;
        case GL_TRIANGLE_STRIP: return VIRGL_PIPE_PRIM_TRIANGLE_STRIP;
        case GL_TRIANGLE_FAN: return VIRGL_PIPE_PRIM_TRIANGLE_FAN;
        default: return 0xffffffffu;
    }
}

static uint32_t gl_attrib_format(GLint size)
{
    switch (size) {
        case 1: return VIRGL_FORMAT_R32_FLOAT;
        case 2: return VIRGL_FORMAT_R32G32_FLOAT;
        case 3: return VIRGL_FORMAT_R32G32B32_FLOAT;
        case 4: return VIRGL_FORMAT_R32G32B32A32_FLOAT;
        default: return 0u;
    }
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    uint32_t commands[128];
    uint32_t element_count = 0u;
    uint32_t vertex_buffer_count = 0u;
    uint32_t cursor = 0u;
    uint32_t primitive = gl_primitive_to_virgl(mode);

    if (!gl_ready || !current_program) { gl_set_error(GL_INVALID_OPERATION); return; }
    if (primitive == 0xffffffffu) { gl_set_error(GL_INVALID_ENUM); return; }
    if (first < 0 || count < 0) { gl_set_error(GL_INVALID_VALUE); return; }
    if (!count) return;

    for (uint32_t i = 0; i < GL_MAX_VERTEX_ATTRIBS_LOCAL; ++i) {
        struct gl_vertex_attrib *a = &gl_attribs[i];
        struct gl_buffer_slot *b;
        uint32_t stride, element_bytes;
        uint64_t last;
        if (!a->enabled) continue;
        b = gl_find_buffer(a->buffer);
        if (!b || !b->created || !b->attached) { gl_set_error(GL_INVALID_OPERATION); return; }
        stride = a->stride ? a->stride : (uint32_t)a->size * 4u;
        element_bytes = (uint32_t)a->size * 4u;
        last = (uint64_t)a->offset + ((uint64_t)(uint32_t)first + (uint32_t)count - 1u) * stride + element_bytes;
        if (last > b->size) { gl_set_error(GL_INVALID_OPERATION); return; }
        ++element_count;
    }
    if (!element_count) { gl_set_error(GL_INVALID_OPERATION); return; }

    if (vertex_elements_created) {
        commands[cursor++] = VIRGL_CMD0(VIRGL_CCMD_DESTROY_OBJECT, VIRGL_OBJECT_VERTEX_ELEMENTS, 1u);
        commands[cursor++] = GL_VERTEX_ELEMENTS_HANDLE;
    }

    commands[cursor++] = VIRGL_CMD0(VIRGL_CCMD_CREATE_OBJECT, VIRGL_OBJECT_VERTEX_ELEMENTS,
                                    1u + element_count * 4u);
    commands[cursor++] = GL_VERTEX_ELEMENTS_HANDLE;
    for (uint32_t i = 0; i < GL_MAX_VERTEX_ATTRIBS_LOCAL; ++i) {
        struct gl_vertex_attrib *a = &gl_attribs[i];
        if (!a->enabled) continue;
        commands[cursor++] = a->offset;
        commands[cursor++] = 0u;
        commands[cursor++] = vertex_buffer_count;
        commands[cursor++] = gl_attrib_format(a->size);
        ++vertex_buffer_count;
    }
    commands[cursor++] = VIRGL_CMD0(VIRGL_CCMD_BIND_OBJECT, VIRGL_OBJECT_VERTEX_ELEMENTS, 1u);
    commands[cursor++] = GL_VERTEX_ELEMENTS_HANDLE;

    commands[cursor++] = VIRGL_CMD0(VIRGL_CCMD_SET_VERTEX_BUFFERS, 0u, vertex_buffer_count * 3u);
    for (uint32_t i = 0; i < GL_MAX_VERTEX_ATTRIBS_LOCAL; ++i) {
        struct gl_vertex_attrib *a = &gl_attribs[i];
        struct gl_buffer_slot *b;
        if (!a->enabled) continue;
        b = gl_find_buffer(a->buffer);
        commands[cursor++] = a->stride ? a->stride : (uint32_t)a->size * 4u;
        commands[cursor++] = 0u;
        commands[cursor++] = b->resource;
    }

    commands[cursor++] = VIRGL_CMD0(VIRGL_CCMD_DRAW_VBO, 0u, 12u);
    commands[cursor++] = (uint32_t)first;
    commands[cursor++] = (uint32_t)count;
    commands[cursor++] = primitive;
    commands[cursor++] = 0u;
    commands[cursor++] = 1u;
    commands[cursor++] = 0u;          /* index bias */
    commands[cursor++] = 0u;          /* start instance */
    commands[cursor++] = 0u;          /* primitive restart */
    commands[cursor++] = 0u;          /* restart index */
    commands[cursor++] = 0u;          /* min index (unused for non-indexed draw) */
    commands[cursor++] = 0xffffffffu; /* max index (unbounded, non-indexed draw) */
    commands[cursor++] = 0u;          /* count from stream output */

    if (cursor > sizeof(commands) / sizeof(commands[0]) ||
        virgl_submit(VIRGL_CONTEXT_DEFAULT, commands, cursor) != 0) {
        gl_set_error(GL_INVALID_OPERATION); return;
    }
    vertex_elements_created = 1;
}


void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
    uint32_t commands[128];
    uint32_t element_count = 0u;
    uint32_t vertex_buffer_count = 0u;
    uint32_t cursor = 0u;
    uint32_t primitive = gl_primitive_to_virgl(mode);
    uint32_t index_size;
    uint32_t index_offset = (uint32_t)(uintptr_t)indices;
    struct gl_buffer_slot *index_buffer;

    if (!gl_ready || !current_program) {
        gl_set_error(GL_INVALID_OPERATION);
        return;
    }
    if (primitive == 0xffffffffu) {
        gl_set_error(GL_INVALID_ENUM);
        return;
    }
    if (count < 0) {
        gl_set_error(GL_INVALID_VALUE);
        return;
    }
    if (!count) return;
    if (type == GL_UNSIGNED_BYTE) index_size = 1u;
    else if (type == GL_UNSIGNED_SHORT) index_size = 2u;
    else if (type == GL_UNSIGNED_INT) index_size = 4u;
    else { gl_set_error(GL_INVALID_ENUM); return; }

    index_buffer = gl_find_buffer(bound_element_array_buffer);
    if (!index_buffer || !index_buffer->created || !index_buffer->attached ||
        index_offset > index_buffer->size ||
        (uint32_t)count > (index_buffer->size - index_offset) / index_size ||
        (index_offset % index_size) != 0u) {
        gl_set_error(GL_INVALID_OPERATION);
        return;
    }

    for (uint32_t i = 0; i < GL_MAX_VERTEX_ATTRIBS_LOCAL; ++i) {
        struct gl_vertex_attrib *a = &gl_attribs[i];
        struct gl_buffer_slot *b;
        uint64_t stride, element_bytes, last;
        if (!a->enabled) continue;
        b = gl_find_buffer(a->buffer);
        if (!b || !b->created || !b->attached) { gl_set_error(GL_INVALID_OPERATION); return; }
        stride = a->stride ? a->stride : (uint32_t)a->size * 4u;
        element_bytes = (uint32_t)a->size * 4u;
        /* Indices stay in the GPU resource. Gallium permits a conservative
           [0, UINT32_MAX] min/max range; it may cost performance but is valid. */
        last = (uint64_t)a->offset + element_bytes;
        if (last > b->size || stride < element_bytes) { gl_set_error(GL_INVALID_OPERATION); return; }
        ++element_count;
    }
    if (!element_count) { gl_set_error(GL_INVALID_OPERATION); return; }

    if (vertex_elements_created) {
        commands[cursor++] = VIRGL_CMD0(VIRGL_CCMD_DESTROY_OBJECT, VIRGL_OBJECT_VERTEX_ELEMENTS, 1u);
        commands[cursor++] = GL_VERTEX_ELEMENTS_HANDLE;
    }

    commands[cursor++] = VIRGL_CMD0(VIRGL_CCMD_CREATE_OBJECT, VIRGL_OBJECT_VERTEX_ELEMENTS,
                                    1u + element_count * 4u);
    commands[cursor++] = GL_VERTEX_ELEMENTS_HANDLE;
    for (uint32_t i = 0; i < GL_MAX_VERTEX_ATTRIBS_LOCAL; ++i) {
        struct gl_vertex_attrib *a = &gl_attribs[i];
        if (!a->enabled) continue;
        commands[cursor++] = a->offset;
        commands[cursor++] = 0u;
        commands[cursor++] = vertex_buffer_count;
        commands[cursor++] = gl_attrib_format(a->size);
        ++vertex_buffer_count;
    }
    commands[cursor++] = VIRGL_CMD0(VIRGL_CCMD_BIND_OBJECT, VIRGL_OBJECT_VERTEX_ELEMENTS, 1u);
    commands[cursor++] = GL_VERTEX_ELEMENTS_HANDLE;

    commands[cursor++] = VIRGL_CMD0(VIRGL_CCMD_SET_VERTEX_BUFFERS, 0u, vertex_buffer_count * 3u);
    for (uint32_t i = 0; i < GL_MAX_VERTEX_ATTRIBS_LOCAL; ++i) {
        struct gl_vertex_attrib *a = &gl_attribs[i];
        struct gl_buffer_slot *b;
        if (!a->enabled) continue;
        b = gl_find_buffer(a->buffer);
        commands[cursor++] = a->stride ? a->stride : (uint32_t)a->size * 4u;
        commands[cursor++] = 0u;
        commands[cursor++] = b->resource;
    }

    commands[cursor++] = VIRGL_CMD0(VIRGL_CCMD_SET_INDEX_BUFFER, 0u, 3u);
    commands[cursor++] = index_buffer->resource;
    commands[cursor++] = index_size;
    commands[cursor++] = index_offset;

    commands[cursor++] = VIRGL_CMD0(VIRGL_CCMD_DRAW_VBO, 0u, 12u);
    commands[cursor++] = 0u;
    commands[cursor++] = (uint32_t)count;
    commands[cursor++] = primitive;
    commands[cursor++] = 1u;
    commands[cursor++] = 1u;
    commands[cursor++] = 0u;
    commands[cursor++] = 0u;
    commands[cursor++] = 0u;
    commands[cursor++] = 0u;
    commands[cursor++] = 0u;
    commands[cursor++] = 0xffffffffu;
    commands[cursor++] = 0u;

    if (cursor > sizeof(commands) / sizeof(commands[0]) ||
        virgl_submit(VIRGL_CONTEXT_DEFAULT, commands, cursor) != 0) {
        gl_set_error(GL_INVALID_OPERATION);
        return;
    }
    vertex_elements_created = 1;
}
