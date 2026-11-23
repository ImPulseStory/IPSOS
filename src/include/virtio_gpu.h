#ifndef IPSOS_VIRTIO_GPU_H
#define IPSOS_VIRTIO_GPU_H

#include <stdint.h>

int virtio_gpu_init(uint32_t **pixels, uint32_t *width, uint32_t *height);
int virtio_gpu_present(void);
int virtio_gpu_present_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
int virtio_gpu_flush_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
int virtio_gpu_cursor_ready(void);
int virtio_gpu_cursor_move(int x, int y);
int virtio_gpu_virgl_supported(void);
int virtio_gpu_virgl_query_capset(uint32_t index, uint32_t *id, uint32_t *version, uint32_t *size);
int virtio_gpu_virgl_get_capset(uint32_t id, uint32_t version, void *buffer, uint32_t capacity, uint32_t *actual_size);
int virtio_gpu_virgl_context_create(uint32_t context_id, const char *name);
int virtio_gpu_virgl_context_destroy(uint32_t context_id);
int virtio_gpu_virgl_create_resource(uint32_t resource_id, uint32_t target, uint32_t format, uint32_t bind, uint32_t width, uint32_t height, uint32_t depth);
int virtio_gpu_virgl_attach_resource(uint32_t context_id, uint32_t resource_id);
int virtio_gpu_virgl_submit(uint32_t context_id, const uint32_t *dwords, uint32_t count);
int virtio_gpu_ready(void);
uint32_t virtio_gpu_last_error(void);
uint32_t virtio_gpu_last_command(void);
uint32_t virtio_gpu_last_response_length(void);

#endif
