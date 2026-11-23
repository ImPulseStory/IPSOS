#ifndef IPSOS_VIRGL_H
#define IPSOS_VIRGL_H

#include <stdint.h>

#define VIRGL_CAPSET_ID 1u
#define VIRGL_CONTEXT_DEFAULT 1u
#define VIRGL_MAX_SUBMIT_DWORDS 4000u

#define VIRGL_CCMD_CREATE_OBJECT 1u
#define VIRGL_CCMD_BIND_OBJECT 2u
#define VIRGL_CCMD_DESTROY_OBJECT 3u
#define VIRGL_CCMD_SET_VIEWPORT_STATE 4u
#define VIRGL_CCMD_SET_FRAMEBUFFER_STATE 5u
#define VIRGL_CCMD_SET_VERTEX_BUFFERS 6u
#define VIRGL_CCMD_CLEAR 7u
#define VIRGL_CCMD_DRAW_VBO 8u
#define VIRGL_CCMD_RESOURCE_INLINE_WRITE 9u
#define VIRGL_CCMD_SET_INDEX_BUFFER 11u
#define VIRGL_CCMD_BIND_SHADER 31u

#define VIRGL_OBJECT_BLEND 1u
#define VIRGL_OBJECT_RASTERIZER 2u
#define VIRGL_OBJECT_DSA 3u
#define VIRGL_OBJECT_SHADER 4u
#define VIRGL_OBJECT_VERTEX_ELEMENTS 5u
#define VIRGL_OBJECT_SURFACE 8u

#define VIRGL_PIPE_BUFFER 0u
#define VIRGL_PIPE_TEXTURE_2D 2u
#define VIRGL_PIPE_SHADER_VERTEX 0u
#define VIRGL_PIPE_SHADER_FRAGMENT 1u
#define VIRGL_PIPE_PRIM_POINTS 0u
#define VIRGL_PIPE_PRIM_LINES 1u
#define VIRGL_PIPE_PRIM_LINE_LOOP 2u
#define VIRGL_PIPE_PRIM_LINE_STRIP 3u
#define VIRGL_PIPE_PRIM_TRIANGLES 4u
#define VIRGL_PIPE_PRIM_TRIANGLE_STRIP 5u
#define VIRGL_PIPE_PRIM_TRIANGLE_FAN 6u
#define VIRGL_FORMAT_Z32_FLOAT 18u
#define VIRGL_FORMAT_R8_UNORM 64u
#define VIRGL_FORMAT_R32_FLOAT 28u
#define VIRGL_FORMAT_R32G32_FLOAT 29u
#define VIRGL_FORMAT_R32G32B32_FLOAT 30u
#define VIRGL_FORMAT_R32G32B32A32_FLOAT 31u
#define VIRGL_MAX_VERTEX_ATTRIBS 8u
#define VIRGL_FORMAT_B8G8R8A8_UNORM 1u
#define VIRGL_FORMAT_B8G8R8X8_UNORM 2u
#define VIRGL_BIND_DEPTH_STENCIL (1u << 0)
#define VIRGL_BIND_RENDER_TARGET (1u << 1)
#define VIRGL_BIND_SAMPLER_VIEW (1u << 3)
#define VIRGL_BIND_VERTEX_BUFFER (1u << 4)
#define VIRGL_BIND_INDEX_BUFFER (1u << 5)
#define VIRGL_BIND_DISPLAY_TARGET (1u << 7)
#define VIRGL_PIPE_CLEAR_DEPTH (1u << 1)
#define VIRGL_PIPE_CLEAR_COLOR0 (1u << 2)
#define VIRGL_CMD0(cmd, object, length) ((uint32_t)(cmd) | ((uint32_t)(object) << 8) | ((uint32_t)(length) << 16))

typedef struct VirglCaps {
    uint32_t capset_id;
    uint32_t max_version;
    uint32_t max_size;
} VirglCaps;

int virgl_init(void);
int virgl_ready(void);
int virgl_get_caps(VirglCaps *caps);
int virgl_get_capset(uint32_t version, void *buffer, uint32_t capacity, uint32_t *actual_size);
int virgl_context_create(uint32_t context_id, const char *name);
int virgl_context_destroy(uint32_t context_id);
int virgl_resource_create_3d(uint32_t resource_id, uint32_t target, uint32_t format, uint32_t bind, uint32_t width, uint32_t height, uint32_t depth);
int virgl_resource_attach(uint32_t context_id, uint32_t resource_id);
int virgl_resource_inline_write(uint32_t resource_id, uint32_t offset, const void *data, uint32_t byte_count);
int virgl_submit(uint32_t context_id, const uint32_t *command_dwords, uint32_t dword_count);
uint32_t virgl_last_error(void);

#endif
