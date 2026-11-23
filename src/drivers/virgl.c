#include "../include/virgl.h"
#include "../include/virtio_gpu.h"

static VirglCaps virgl_caps;
static int virgl_is_ready;
static uint32_t virgl_error;

int virgl_init(void)
{
    uint32_t id = 0;
    uint32_t version = 0;
    uint32_t size = 0;

    if (virgl_is_ready) return 0;
    virgl_is_ready = 0;
    virgl_error = 0;
    virgl_caps.capset_id = 0;
    virgl_caps.max_version = 0;
    virgl_caps.max_size = 0;

    if (!virtio_gpu_ready() || !virtio_gpu_virgl_supported()) {
        virgl_error = 1;
        return -1;
    }
    if (virtio_gpu_virgl_query_capset(0, &id, &version, &size) != 0 ||
        id != VIRGL_CAPSET_ID || version == 0 || size == 0) {
        virgl_error = 2;
        return -1;
    }

    virgl_caps.capset_id = id;
    virgl_caps.max_version = version;
    virgl_caps.max_size = size;

    if (virtio_gpu_virgl_context_create(VIRGL_CONTEXT_DEFAULT, "IPSOS-VirGL") != 0) {
        virgl_error = 3;
        return -1;
    }

    virgl_is_ready = 1;
    return 0;
}

int virgl_ready(void)
{
    return virgl_is_ready;
}

int virgl_get_caps(VirglCaps *caps)
{
    if (!caps || !virgl_is_ready) return -1;
    *caps = virgl_caps;
    return 0;
}

int virgl_get_capset(uint32_t version, void *buffer, uint32_t capacity, uint32_t *actual_size)
{
    if (!virgl_is_ready || version > virgl_caps.max_version || capacity < virgl_caps.max_size)
        return -1;
    return virtio_gpu_virgl_get_capset(virgl_caps.capset_id, version, buffer, capacity, actual_size);
}

int virgl_context_create(uint32_t context_id, const char *name)
{
    if (!virtio_gpu_virgl_supported() || context_id != VIRGL_CONTEXT_DEFAULT || !name) return -1;
    if (virgl_is_ready) return 0;
    if (virtio_gpu_virgl_context_create(context_id, name) != 0) return -1;
    virgl_is_ready = 1;
    return 0;
}

int virgl_context_destroy(uint32_t context_id)
{
    if (context_id != VIRGL_CONTEXT_DEFAULT || !virgl_is_ready) return -1;
    if (virtio_gpu_virgl_context_destroy(context_id) != 0) return -1;
    virgl_is_ready = 0;
    return 0;
}

int virgl_resource_create_3d(uint32_t resource_id, uint32_t target, uint32_t format, uint32_t bind, uint32_t width, uint32_t height, uint32_t depth)
{
    if (!virgl_is_ready) return -1;
    return virtio_gpu_virgl_create_resource(resource_id, target, format, bind, width, height, depth);
}

int virgl_resource_attach(uint32_t context_id, uint32_t resource_id)
{
    if (!virgl_is_ready || context_id != VIRGL_CONTEXT_DEFAULT) return -1;
    return virtio_gpu_virgl_attach_resource(context_id, resource_id);
}

int virgl_submit(uint32_t context_id, const uint32_t *command_dwords, uint32_t dword_count)
{
    if (!virgl_is_ready || context_id != VIRGL_CONTEXT_DEFAULT || dword_count > VIRGL_MAX_SUBMIT_DWORDS) return -1;
    return virtio_gpu_virgl_submit(context_id, command_dwords, dword_count);
}

uint32_t virgl_last_error(void)
{
    return virgl_error ? virgl_error : virtio_gpu_last_error();
}


int virgl_resource_inline_write(uint32_t resource_id, uint32_t offset,
                                const void *data, uint32_t byte_count)
{
    const uint8_t *source = (const uint8_t *)data;
    uint32_t packet[VIRGL_MAX_SUBMIT_DWORDS];

    if (!virgl_is_ready || !resource_id || (!data && byte_count))
        return -1;

    while (byte_count) {
        uint32_t max_chunk = (VIRGL_MAX_SUBMIT_DWORDS - 12u) * 4u;
        uint32_t chunk = byte_count > max_chunk ? max_chunk : byte_count;
        uint32_t data_dwords = (chunk + 3u) / 4u;
        uint32_t total_dwords = 12u + data_dwords;
        uint8_t *dst;

        packet[0] = VIRGL_CMD0(VIRGL_CCMD_RESOURCE_INLINE_WRITE, 0u, 11u + data_dwords);
        packet[1] = resource_id;
        packet[2] = 0u; /* mip level */
        packet[3] = 0u; /* usage: match Gallium inline-write buffer uploads */
        packet[4] = 0u; /* row stride: tightly packed buffer */
        packet[5] = 0u; /* layer stride */
        packet[6] = offset;
        packet[7] = 0u;
        packet[8] = 0u;
        packet[9] = chunk;
        packet[10] = 1u;
        packet[11] = 1u;
        dst = (uint8_t *)&packet[12];
        for (uint32_t i = 0; i < data_dwords * 4u; ++i)
            dst[i] = i < chunk ? source[i] : 0u;

        if (virgl_submit(VIRGL_CONTEXT_DEFAULT, packet, total_dwords) != 0)
            return -1;
        source += chunk;
        offset += chunk;
        byte_count -= chunk;
    }
    return 0;
}
