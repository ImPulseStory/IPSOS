#include "../include/virtio_gpu.h"
#include "../include/pci.h"
#include "../include/heap.h"
#include <stdint.h>
#include <stddef.h>

#define VIRTIO_PCI_VENDOR_ID       0x1AF4u
#define VIRTIO_GPU_DEVICE_ID       0x1050u
#define PCI_CAP_ID_VENDOR          0x09u
#define VIRTIO_PCI_CAP_COMMON      1u
#define VIRTIO_PCI_CAP_NOTIFY      2u
#define VIRTIO_F_VERSION_1         32u
#define VIRTIO_GPU_F_VIRGL          0u
#define GPU_CAPSET_VIRGL             1u
#define GPU_VIRGL_CONTEXT_ID         1u

#define VIRTIO_STATUS_ACKNOWLEDGE  1u
#define VIRTIO_STATUS_DRIVER       2u
#define VIRTIO_STATUS_DRIVER_OK    4u
#define VIRTIO_STATUS_FEATURES_OK  8u
#define VIRTIO_STATUS_FAILED       128u

#define VIRTQ_SIZE                 128u
#define VIRTQ_DESC_F_NEXT          1u
#define VIRTQ_DESC_F_WRITE         2u
#define GPU_COMMAND_TIMEOUT        10000000u
#define GPU_MAX_BUFFER_BYTES       (8u * 1024u * 1024u)
#define GPU_PAGE_SIZE              4096u
#define GPU_RESOURCE_ID            1u
#define GPU_CURSOR_RESOURCE_ID      2u
#define GPU_CURSOR_SIZE             64u
#define GPU_FLAG_FENCE              1u

#define GPU_CMD_GET_DISPLAY_INFO       0x0100u
#define GPU_CMD_RESOURCE_CREATE_2D     0x0101u
#define GPU_CMD_SET_SCANOUT            0x0103u
#define GPU_CMD_RESOURCE_FLUSH         0x0104u
#define GPU_CMD_TRANSFER_TO_HOST_2D    0x0105u
#define GPU_CMD_RESOURCE_ATTACH_BACKING 0x0106u
#define GPU_CMD_GET_CAPSET_INFO        0x0108u
#define GPU_CMD_GET_CAPSET             0x0109u
#define GPU_CMD_CTX_CREATE             0x0200u
#define GPU_CMD_CTX_DESTROY            0x0201u
#define GPU_CMD_CTX_ATTACH_RESOURCE    0x0202u
#define GPU_CMD_CTX_DETACH_RESOURCE    0x0203u
#define GPU_CMD_RESOURCE_CREATE_3D     0x0204u
#define GPU_CMD_SUBMIT_3D              0x0207u
#define GPU_CMD_UPDATE_CURSOR          0x0300u
#define GPU_CMD_MOVE_CURSOR            0x0301u

#define GPU_RESP_OK_NODATA          0x1100u
#define GPU_RESP_OK_DISPLAY_INFO    0x1101u
#define GPU_FORMAT_B8G8R8X8_UNORM   2u
#define GPU_MAX_SCANOUTS            16u

#define PACKED __attribute__((packed))
#define ALIGNED(x) __attribute__((aligned(x)))

typedef struct PACKED {
    uint64_t address;
    uint32_t length;
    uint16_t flags;
    uint16_t next;
} virtq_desc_t;

typedef struct PACKED {
    volatile uint16_t flags;
    volatile uint16_t index;
    volatile uint16_t ring[VIRTQ_SIZE];
    volatile uint16_t used_event;
} virtq_avail_t;

typedef struct PACKED {
    uint32_t id;
    uint32_t length;
} virtq_used_element_t;

typedef struct PACKED {
    volatile uint16_t flags;
    volatile uint16_t index;
    volatile virtq_used_element_t ring[VIRTQ_SIZE];
    volatile uint16_t available_event;
} virtq_used_t;

typedef struct PACKED {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t context_id;
    uint8_t ring_index;
    uint8_t padding[3];
} gpu_ctrl_header_t;

typedef struct PACKED {
    gpu_ctrl_header_t header;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} gpu_rect_command_t;

typedef struct PACKED {
    gpu_ctrl_header_t header;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
} gpu_create_2d_t;

typedef struct PACKED {
    gpu_ctrl_header_t header;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t scanout_id;
    uint32_t resource_id;
} gpu_set_scanout_t;

typedef struct PACKED {
    gpu_ctrl_header_t header;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t padding;
} gpu_transfer_2d_t;

typedef struct PACKED {
    gpu_ctrl_header_t header;
    uint32_t resource_id;
    uint32_t entry_count;
} gpu_attach_backing_t;

typedef struct PACKED {
    uint64_t address;
    uint32_t length;
    uint32_t padding;
} gpu_memory_entry_t;

typedef struct PACKED {
    gpu_attach_backing_t command;
    gpu_memory_entry_t entry;
} gpu_attach_backing_full_t;

typedef struct PACKED {
    gpu_ctrl_header_t header;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t resource_id;
    uint32_t padding;
} gpu_resource_flush_t;

typedef struct PACKED {
    struct PACKED {
        uint32_t x;
        uint32_t y;
        uint32_t width;
        uint32_t height;
    } rect;
    uint32_t enabled;
    uint32_t flags;
} gpu_display_one_t;

typedef struct PACKED {
    gpu_ctrl_header_t header;
    gpu_display_one_t modes[GPU_MAX_SCANOUTS];
} gpu_display_info_t;

typedef struct PACKED {
    gpu_ctrl_header_t header;
    uint32_t capset_index;
} gpu_get_capset_info_t;

typedef struct PACKED {
    gpu_ctrl_header_t header;
    uint32_t capset_id;
    uint32_t version;
} gpu_get_capset_t;

typedef struct PACKED {
    gpu_ctrl_header_t header;
    uint32_t capset_id;
    uint32_t max_version;
    uint32_t max_size;
} gpu_capset_info_response_t;

typedef struct PACKED {
    gpu_ctrl_header_t header;
    uint32_t name_length;
    uint32_t context_init;
    char debug_name[64];
} gpu_context_create_t;

typedef struct PACKED {
    gpu_ctrl_header_t header;
    uint32_t resource_id;
    uint32_t padding;
} gpu_context_resource_t;

typedef struct PACKED {
    gpu_ctrl_header_t header;
    uint32_t resource_id;
    uint32_t target;
    uint32_t format;
    uint32_t bind;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t array_size;
    uint32_t last_level;
    uint32_t samples;
    uint32_t flags;
    uint32_t padding;
} gpu_resource_create_3d_t;

typedef struct PACKED {
    gpu_ctrl_header_t header;
    uint32_t size;
    uint32_t padding;
} gpu_submit_3d_t;

typedef struct PACKED {
    uint32_t x;
    uint32_t y;
    uint32_t scanout_id;
} gpu_cursor_pos_t;

typedef struct PACKED {
    gpu_ctrl_header_t header;
    gpu_cursor_pos_t position;
    uint32_t resource_id;
    uint32_t hot_x;
    uint32_t hot_y;
    uint32_t padding;
} gpu_update_cursor_t;

static void gpu_header_init(gpu_ctrl_header_t *header, uint32_t command_type);
static int gpu_send_simple(void *command, uint32_t command_length, uint32_t expected_response);

typedef char virtq_desc_size_must_be_16[(sizeof(virtq_desc_t) == 16u) ? 1 : -1];
typedef char gpu_header_size_must_be_24[(sizeof(gpu_ctrl_header_t) == 24u) ? 1 : -1];
typedef char gpu_create_size_must_be_40[(sizeof(gpu_create_2d_t) == 40u) ? 1 : -1];
typedef char gpu_scanout_size_must_be_48[(sizeof(gpu_set_scanout_t) == 48u) ? 1 : -1];
typedef char gpu_flush_size_must_be_48[(sizeof(gpu_resource_flush_t) == 48u) ? 1 : -1];
typedef char gpu_display_info_size_must_be_408[(sizeof(gpu_display_info_t) == 408u) ? 1 : -1];
typedef char gpu_attach_size_must_be_48[(sizeof(gpu_attach_backing_full_t) == 48u) ? 1 : -1];
typedef char gpu_capset_info_size_must_be_28[(sizeof(gpu_get_capset_info_t) == 28u) ? 1 : -1];
typedef char gpu_capset_info_response_size_must_be_36[(sizeof(gpu_capset_info_response_t) == 36u) ? 1 : -1];
typedef char gpu_resource_create_3d_size_must_be_72[(sizeof(gpu_resource_create_3d_t) == 72u) ? 1 : -1];
typedef char gpu_submit_3d_size_must_be_32[(sizeof(gpu_submit_3d_t) == 32u) ? 1 : -1];
typedef char gpu_update_cursor_size_must_be_52[(sizeof(gpu_update_cursor_t) == 52u) ? 1 : -1];
typedef char gpu_transfer_size_must_be_56[(sizeof(gpu_transfer_2d_t) == 56u) ? 1 : -1];

static virtq_desc_t queue_descriptors[VIRTQ_SIZE] ALIGNED(16);
static virtq_avail_t queue_available ALIGNED(2);
static virtq_used_t queue_used ALIGNED(4);
static virtq_desc_t cursor_queue_descriptors[VIRTQ_SIZE] ALIGNED(16);
static virtq_avail_t cursor_queue_available ALIGNED(2);
static virtq_used_t cursor_queue_used ALIGNED(4);
static uint8_t command_request[16384] ALIGNED(16);
static uint8_t command_response[8192] ALIGNED(16);
static uint8_t virgl_submit_request[16384] ALIGNED(16);

static volatile uint8_t *common_config;
static volatile uint8_t *notify_region;
static uint32_t notify_region_length;
static uint32_t notify_multiplier;
static uintptr_t notify_register;
static uint16_t queue_size;
static uint16_t last_used_index;
static uint16_t cursor_queue_size;
static uint16_t cursor_last_used_index;
static uintptr_t cursor_notify_register;
static uint32_t *gpu_pixels;
static uint32_t *gpu_cursor_pixels;
static void *gpu_cursor_allocation;
static uint32_t gpu_cursor_fence;
static int gpu_cursor_is_ready;
static void *gpu_allocation;
static uint32_t gpu_width;
static uint32_t gpu_height;
static uint32_t gpu_buffer_size;
static uint32_t gpu_last_error;
static uint32_t gpu_last_command;
static uint32_t gpu_last_response_length;
static int gpu_is_ready;
static int gpu_virgl_supported;
static int gpu_virgl_context_ready;

static inline void memory_barrier(void)
{
    __asm__ volatile("" : : : "memory");
}

static inline uint32_t irq_save(void)
{
    uint32_t flags;
    __asm__ volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static inline void irq_restore(uint32_t flags)
{
    if (flags & 0x200u)
        __asm__ volatile("sti" : : : "memory");
}

static uint8_t mmio_read8(uintptr_t address)
{
    return *(volatile uint8_t *)address;
}

static uint16_t mmio_read16(uintptr_t address)
{
    return *(volatile uint16_t *)address;
}

static uint32_t mmio_read32(uintptr_t address)
{
    return *(volatile uint32_t *)address;
}

static void mmio_write8(uintptr_t address, uint8_t value)
{
    *(volatile uint8_t *)address = value;
}

static void mmio_write16(uintptr_t address, uint16_t value)
{
    *(volatile uint16_t *)address = value;
}

static void mmio_write32(uintptr_t address, uint32_t value)
{
    *(volatile uint32_t *)address = value;
}

static void mmio_write64(uintptr_t address, uint64_t value)
{
    mmio_write32(address, (uint32_t)value);
    mmio_write32(address + 4u, (uint32_t)(value >> 32));
}

static void zero_bytes(void *memory, uint32_t length)
{
    uint8_t *bytes = (uint8_t *)memory;
    for (uint32_t i = 0; i < length; ++i)
        bytes[i] = 0;
}

static void gpu_set_error(uint32_t error)
{
    if (!gpu_last_error)
        gpu_last_error = error;
    if (common_config) {
        uint8_t status = mmio_read8((uintptr_t)common_config + 20u);
        mmio_write8((uintptr_t)common_config + 20u, (uint8_t)(status | VIRTIO_STATUS_FAILED));
    }
}

static int pci_bar_memory_base(const PciDevice *device, uint8_t bar_index, uintptr_t *base)
{
    uint32_t low;
    uint32_t high = 0;
    uint32_t type;

    if (!device || !base || bar_index > 5u)
        return -1;

    low = pci_read_config32(device, (uint8_t)(0x10u + bar_index * 4u));
    if (low == 0u || low == 0xFFFFFFFFu || (low & 1u))
        return -1;

    type = (low >> 1) & 3u;
    if (type == 2u) {
        if (bar_index >= 5u)
            return -1;
        high = pci_read_config32(device, (uint8_t)(0x14u + bar_index * 4u));
        if (high != 0u)
            return -1;
    } else if (type != 0u) {
        return -1;
    }

    *base = (uintptr_t)(low & 0xFFFFFFF0u);
    return *base ? 0 : -1;
}

static int map_virtio_capability(const PciDevice *device, uint8_t wanted_type,
                                 volatile uint8_t **mapped, uint32_t *mapped_length,
                                 uint32_t *extra)
{
    uint8_t pointer;
    uint16_t status;
    uint32_t guard = 0;

    if (!device || !mapped || !mapped_length)
        return -1;

    status = pci_read_config16(device, 0x06u);
    if (!(status & 0x10u))
        return -1;

    pointer = (uint8_t)(pci_read_config8(device, 0x34u) & 0xFCu);
    while (pointer >= 0x40u && guard++ < 48u) {
        uint8_t capability_id = pci_read_config8(device, pointer);
        uint8_t next = (uint8_t)(pci_read_config8(device, (uint8_t)(pointer + 1u)) & 0xFCu);

        if (capability_id == PCI_CAP_ID_VENDOR) {
            uint8_t capability_length = pci_read_config8(device, (uint8_t)(pointer + 2u));
            uint8_t capability_type = pci_read_config8(device, (uint8_t)(pointer + 3u));
            uint8_t bar = pci_read_config8(device, (uint8_t)(pointer + 4u));
            uint32_t offset = pci_read_config32(device, (uint8_t)(pointer + 8u));
            uint32_t length = pci_read_config32(device, (uint8_t)(pointer + 12u));

            if (capability_type == wanted_type) {
                uintptr_t bar_base;
                uint32_t multiplier = 0;
                uint32_t required = wanted_type == VIRTIO_PCI_CAP_NOTIFY ? 20u : 16u;

                if (capability_length < required ||
                    (uint32_t)pointer + capability_length > 0x100u ||
                    !length || bar > 5u ||
                    offset > 0xFFFFFFFFu - length ||
                    pci_bar_memory_base(device, bar, &bar_base) != 0 ||
                    bar_base > 0xFFFFFFFFu - offset ||
                    bar_base + offset > 0xFFFFFFFFu - length)
                    return -1;

                if (wanted_type == VIRTIO_PCI_CAP_NOTIFY) {
                    if (capability_length < 20u)
                        return -1;
                    multiplier = pci_read_config32(device, (uint8_t)(pointer + 16u));
                }

                *mapped = (volatile uint8_t *)(bar_base + offset);
                *mapped_length = length;
                if (extra)
                    *extra = multiplier;
                return 0;
            }
        }

        if (!next || next == pointer)
            break;
        pointer = next;
    }

    return -1;
}

static int virtio_setup_queue(void)
{
    uint16_t maximum_size;
    uint16_t notify_offset;
    uint64_t queue_bytes;

    if (mmio_read16((uintptr_t)common_config + 18u) < 1u)
        return -1;

    mmio_write16((uintptr_t)common_config + 22u, 0u);
    maximum_size = mmio_read16((uintptr_t)common_config + 24u);
    if (maximum_size < 2u)
        return -1;

    queue_size = maximum_size < VIRTQ_SIZE ? maximum_size : VIRTQ_SIZE;
    while (queue_size > 2u && (queue_size & (uint16_t)(queue_size - 1u)))
        --queue_size;

    zero_bytes(queue_descriptors, sizeof(queue_descriptors));
    zero_bytes((void *)&queue_available, sizeof(queue_available));
    zero_bytes((void *)&queue_used, sizeof(queue_used));

    mmio_write16((uintptr_t)common_config + 24u, queue_size);
    mmio_write64((uintptr_t)common_config + 32u, (uint64_t)(uintptr_t)queue_descriptors);
    mmio_write64((uintptr_t)common_config + 40u, (uint64_t)(uintptr_t)&queue_available);
    mmio_write64((uintptr_t)common_config + 48u, (uint64_t)(uintptr_t)&queue_used);

    notify_offset = mmio_read16((uintptr_t)common_config + 30u);
    queue_bytes = (uint64_t)notify_offset * notify_multiplier;
    if (queue_bytes > notify_region_length ||
        (uint64_t)notify_region_length - queue_bytes < 2u ||
        (uintptr_t)notify_region > 0xFFFFFFFFu - (uintptr_t)queue_bytes)
        return -1;
    notify_register = (uintptr_t)notify_region + (uintptr_t)queue_bytes;

    memory_barrier();
    mmio_write16((uintptr_t)common_config + 28u, 1u);
    if (mmio_read16((uintptr_t)common_config + 28u) != 1u)
        return -1;

    last_used_index = 0;
    return 0;
}

static int virtio_setup_cursor_queue(void)
{
    uint16_t maximum_size;
    uint16_t notify_offset;
    uint64_t queue_bytes;

    if (mmio_read16((uintptr_t)common_config + 18u) < 2u) return -1;
    mmio_write16((uintptr_t)common_config + 22u, 1u);
    maximum_size = mmio_read16((uintptr_t)common_config + 24u);
    if (maximum_size < 2u) return -1;
    cursor_queue_size = maximum_size < VIRTQ_SIZE ? maximum_size : VIRTQ_SIZE;
    while (cursor_queue_size > 2u && (cursor_queue_size & (uint16_t)(cursor_queue_size - 1u)))
        --cursor_queue_size;

    zero_bytes(cursor_queue_descriptors, sizeof(cursor_queue_descriptors));
    zero_bytes((void *)&cursor_queue_available, sizeof(cursor_queue_available));
    zero_bytes((void *)&cursor_queue_used, sizeof(cursor_queue_used));
    mmio_write16((uintptr_t)common_config + 24u, cursor_queue_size);
    mmio_write64((uintptr_t)common_config + 32u, (uint64_t)(uintptr_t)cursor_queue_descriptors);
    mmio_write64((uintptr_t)common_config + 40u, (uint64_t)(uintptr_t)&cursor_queue_available);
    mmio_write64((uintptr_t)common_config + 48u, (uint64_t)(uintptr_t)&cursor_queue_used);
    notify_offset = mmio_read16((uintptr_t)common_config + 30u);
    queue_bytes = (uint64_t)notify_offset * notify_multiplier;
    if (queue_bytes > notify_region_length ||
        (uint64_t)notify_region_length - queue_bytes < 2u ||
        (uintptr_t)notify_region > 0xFFFFFFFFu - (uintptr_t)queue_bytes) return -1;
    cursor_notify_register = (uintptr_t)notify_region + (uintptr_t)queue_bytes;
    memory_barrier();
    mmio_write16((uintptr_t)common_config + 28u, 1u);
    if (mmio_read16((uintptr_t)common_config + 28u) != 1u) return -1;
    cursor_last_used_index = 0;
    return 0;
}

static int virtio_submit_cursor_command(const void *request, uint32_t request_length)
{
    uint32_t flags;
    uint16_t available_index;
    uint16_t observed_index;
    uint32_t wait;
    int result = -1;

    if (!cursor_notify_register || !request || !request_length || request_length > sizeof(command_request))
        return -1;
    flags = irq_save();
    zero_bytes(command_request, sizeof(command_request));
    zero_bytes(command_response, sizeof(command_response));
    for (uint32_t i = 0; i < request_length; ++i) command_request[i] = ((const uint8_t *)request)[i];
    cursor_queue_descriptors[0].address = (uint64_t)(uintptr_t)command_request;
    cursor_queue_descriptors[0].length = request_length;
    cursor_queue_descriptors[0].flags = VIRTQ_DESC_F_NEXT;
    cursor_queue_descriptors[0].next = 1u;
    cursor_queue_descriptors[1].address = (uint64_t)(uintptr_t)command_response;
    cursor_queue_descriptors[1].length = sizeof(gpu_ctrl_header_t);
    cursor_queue_descriptors[1].flags = VIRTQ_DESC_F_WRITE;
    cursor_queue_descriptors[1].next = 0u;
    available_index = cursor_queue_available.index;
    cursor_queue_available.ring[available_index % cursor_queue_size] = 0u;
    memory_barrier();
    cursor_queue_available.index = (uint16_t)(available_index + 1u);
    memory_barrier();
    mmio_write16(cursor_notify_register, 1u);
    observed_index = cursor_last_used_index;
    for (wait = 0; wait < GPU_COMMAND_TIMEOUT; ++wait) {
        if (cursor_queue_used.index != observed_index) break;
        __asm__ volatile("pause");
    }
    if (wait == GPU_COMMAND_TIMEOUT) { gpu_last_error = 0xE101u; goto done; }
    memory_barrier();
    if (cursor_queue_used.ring[observed_index % cursor_queue_size].id != 0u ||
        cursor_queue_used.ring[observed_index % cursor_queue_size].length < sizeof(gpu_ctrl_header_t)) {
        gpu_last_error = 0xE102u; goto done;
    }
    ++cursor_last_used_index;
    if (*(uint32_t *)command_response != GPU_RESP_OK_NODATA) {
        gpu_last_error = *(uint32_t *)command_response; goto done;
    }
    result = 0;
done:
    irq_restore(flags);
    return result;
}

static int gpu_create_hardware_cursor(void)
{
    gpu_create_2d_t create;
    gpu_attach_backing_full_t attach;
    gpu_transfer_2d_t transfer;
    gpu_update_cursor_t update;
    uint32_t bytes = GPU_CURSOR_SIZE * GPU_CURSOR_SIZE * 4u;
    uint32_t allocation_size = bytes + GPU_PAGE_SIZE - 1u;
    uintptr_t aligned;

    gpu_cursor_allocation = kmalloc(allocation_size);
    if (!gpu_cursor_allocation) return -1;
    aligned = ((uintptr_t)gpu_cursor_allocation + GPU_PAGE_SIZE - 1u) & ~(uintptr_t)(GPU_PAGE_SIZE - 1u);
    if (aligned < 0x00100000u || aligned > 0xFFFFFFFFu) return -1;
    gpu_cursor_pixels = (uint32_t *)aligned;
    zero_bytes(gpu_cursor_pixels, bytes);

    for (uint32_t y = 0; y < 20u; ++y) {
        for (uint32_t x = 0; x <= y / 2u && x < 10u; ++x) {
            uint32_t color = (x == 0u || x == y / 2u) ? 0xFF101010u : 0xFFF8F8F8u;
            gpu_cursor_pixels[y * GPU_CURSOR_SIZE + x] = color;
        }
    }
    for (uint32_t y = 10u; y < 24u; ++y) {
        for (uint32_t x = 4u; x < 12u; ++x) {
            if (x == 4u || x == 11u || y == 23u) gpu_cursor_pixels[y * GPU_CURSOR_SIZE + x] = 0xFF101010u;
            else gpu_cursor_pixels[y * GPU_CURSOR_SIZE + x] = 0xFFF8F8F8u;
        }
    }

    gpu_header_init(&create.header, GPU_CMD_RESOURCE_CREATE_2D);
    create.resource_id = GPU_CURSOR_RESOURCE_ID;
    create.format = 1u;
    create.width = GPU_CURSOR_SIZE;
    create.height = GPU_CURSOR_SIZE;
    if (gpu_send_simple(&create, sizeof(create), GPU_RESP_OK_NODATA) != 0) return -1;

    gpu_header_init(&attach.command.header, GPU_CMD_RESOURCE_ATTACH_BACKING);
    attach.command.resource_id = GPU_CURSOR_RESOURCE_ID;
    attach.command.entry_count = 1u;
    attach.entry.address = (uint64_t)(uintptr_t)gpu_cursor_pixels;
    attach.entry.length = bytes;
    attach.entry.padding = 0;
    if (gpu_send_simple(&attach, sizeof(attach), GPU_RESP_OK_NODATA) != 0) return -1;

    gpu_header_init(&transfer.header, GPU_CMD_TRANSFER_TO_HOST_2D);
    transfer.header.flags = GPU_FLAG_FENCE;
    transfer.header.fence_id = ++gpu_cursor_fence;
    transfer.x = 0; transfer.y = 0;
    transfer.width = GPU_CURSOR_SIZE; transfer.height = GPU_CURSOR_SIZE;
    transfer.offset = 0; transfer.resource_id = GPU_CURSOR_RESOURCE_ID; transfer.padding = 0;
    if (gpu_send_simple(&transfer, sizeof(transfer), GPU_RESP_OK_NODATA) != 0) return -1;

    zero_bytes(&update, sizeof(update));
    gpu_header_init(&update.header, GPU_CMD_UPDATE_CURSOR);
    update.position.x = 0; update.position.y = 0; update.position.scanout_id = 0;
    update.resource_id = GPU_CURSOR_RESOURCE_ID;
    return virtio_submit_cursor_command(&update, sizeof(update));
}

static int virtio_submit_command(const void *request, uint32_t request_length,
                                 void *response, uint32_t response_length)
{
    uint32_t flags;
    uint16_t available_index;
    uint16_t observed_index;
    uint32_t wait;
    int result = -1;

    if (!gpu_is_ready && (!common_config || !notify_register))
        return -1;
    if (!request || !response || !request_length || !response_length ||
        request_length > sizeof(command_request) || response_length > sizeof(command_response))
        return -1;

    flags = irq_save();
    gpu_last_response_length = 0u;
    zero_bytes(command_request, sizeof(command_request));
    zero_bytes(command_response, sizeof(command_response));
    {
        const uint8_t *src = (const uint8_t *)request;
        for (uint32_t i = 0; i < request_length; ++i)
            command_request[i] = src[i];
    }

    gpu_last_command = *(const uint32_t *)command_request;
    queue_descriptors[0].address = (uint64_t)(uintptr_t)command_request;
    queue_descriptors[0].length = request_length;
    queue_descriptors[0].flags = VIRTQ_DESC_F_NEXT;
    queue_descriptors[0].next = 1u;

    queue_descriptors[1].address = (uint64_t)(uintptr_t)command_response;
    queue_descriptors[1].length = response_length;
    queue_descriptors[1].flags = VIRTQ_DESC_F_WRITE;
    queue_descriptors[1].next = 0u;

    available_index = queue_available.index;
    queue_available.ring[available_index % queue_size] = 0u;
    memory_barrier();
    queue_available.index = (uint16_t)(available_index + 1u);
    memory_barrier();
    mmio_write16(notify_register, 0u);

    observed_index = last_used_index;
    for (wait = 0; wait < GPU_COMMAND_TIMEOUT; ++wait) {
        if (queue_used.index != observed_index)
            break;
        __asm__ volatile("pause");
    }

    if (wait == GPU_COMMAND_TIMEOUT) {
        gpu_last_error = 0xE001u;
        goto done;
    }

    memory_barrier();
    if (queue_used.ring[observed_index % queue_size].id != 0u) {
        gpu_last_error = 0xE002u;
        goto done;
    }

    gpu_last_response_length = queue_used.ring[observed_index % queue_size].length;
    if (gpu_last_response_length < sizeof(gpu_ctrl_header_t)) {
        gpu_last_error = 0xE003u;
        goto done;
    }

    ++last_used_index;
    /* VirtIO GPU error responses contain only the control header. Parse the
       response type before requiring the full success-response structure. */
    if (*(uint32_t *)command_response >= 0x1200u) {
        gpu_last_error = *(uint32_t *)command_response;
        goto done;
    }
    if (gpu_last_response_length < response_length) {
        gpu_last_error = 0xE003u;
        goto done;
    }

    if (response != command_response) {
        uint8_t *dst = (uint8_t *)response;
        for (uint32_t i = 0; i < response_length; ++i)
            dst[i] = command_response[i];
    }
    result = 0;

done:
    irq_restore(flags);
    return result;
}

static void gpu_header_init(gpu_ctrl_header_t *header, uint32_t command_type)
{
    header->type = command_type;
    header->flags = 0;
    header->fence_id = 0;
    header->context_id = 0;
    header->ring_index = 0;
    header->padding[0] = 0;
    header->padding[1] = 0;
    header->padding[2] = 0;
}

static int gpu_send_simple(void *command, uint32_t command_length, uint32_t expected_response)
{
    gpu_ctrl_header_t response;
    int result = virtio_submit_command(command, command_length, &response, sizeof(response));
    if (result != 0)
        return -1;
    if (response.type != expected_response) {
        gpu_last_error = 0xE004u;
        return -1;
    }
    return 0;
}

static int gpu_query_display(uint32_t *width, uint32_t *height)
{
    gpu_ctrl_header_t request;
    gpu_display_info_t response;

    gpu_header_init(&request, GPU_CMD_GET_DISPLAY_INFO);
    if (virtio_submit_command(&request, sizeof(request), &response, sizeof(response)) != 0)
        return -1;
    if (response.header.type != GPU_RESP_OK_DISPLAY_INFO ||
        !response.modes[0].enabled || !response.modes[0].rect.width || !response.modes[0].rect.height)
        return -1;

    *width = response.modes[0].rect.width;
    *height = response.modes[0].rect.height;
    return 0;
}

static int gpu_create_scanout(void)
{
    gpu_create_2d_t create;
    gpu_attach_backing_full_t attach;
    gpu_set_scanout_t scanout;

    gpu_header_init(&create.header, GPU_CMD_RESOURCE_CREATE_2D);
    create.resource_id = GPU_RESOURCE_ID;
    create.format = GPU_FORMAT_B8G8R8X8_UNORM;
    create.width = gpu_width;
    create.height = gpu_height;
    if (gpu_send_simple(&create, sizeof(create), GPU_RESP_OK_NODATA) != 0)
        return -1;

    gpu_header_init(&attach.command.header, GPU_CMD_RESOURCE_ATTACH_BACKING);
    attach.command.resource_id = GPU_RESOURCE_ID;
    attach.command.entry_count = 1u;
    attach.entry.address = (uint64_t)(uintptr_t)gpu_pixels;
    attach.entry.length = gpu_buffer_size;
    attach.entry.padding = 0;
    if (gpu_send_simple(&attach, sizeof(attach), GPU_RESP_OK_NODATA) != 0)
        return -1;

    gpu_header_init(&scanout.header, GPU_CMD_SET_SCANOUT);
    scanout.x = 0;
    scanout.y = 0;
    scanout.width = gpu_width;
    scanout.height = gpu_height;
    scanout.scanout_id = 0;
    scanout.resource_id = GPU_RESOURCE_ID;
    return gpu_send_simple(&scanout, sizeof(scanout), GPU_RESP_OK_NODATA);
}

int virtio_gpu_init(uint32_t **pixels, uint32_t *width, uint32_t *height)
{
    PciDevice device;
    uint16_t command;
    uint64_t features;
    uint8_t status;
    uint32_t common_length = 0;
    uint32_t notify_length = 0;
    uint32_t ignored_extra = 0;
    uint32_t byte_count;
    uint32_t backing_size;
    uintptr_t aligned_address;

    gpu_is_ready = 0;
    gpu_pixels = 0;
    gpu_allocation = 0;
    gpu_width = 0;
    gpu_height = 0;
    gpu_buffer_size = 0;
    gpu_last_error = 0;
    gpu_last_command = 0;
    gpu_virgl_supported = 0;
    gpu_virgl_context_ready = 0;
    gpu_cursor_is_ready = 0;
    gpu_cursor_pixels = 0;
    gpu_cursor_allocation = 0;
    cursor_notify_register = 0;
    common_config = 0;
    notify_region = 0;
    notify_register = 0;

    if (pci_find_device(VIRTIO_PCI_VENDOR_ID, VIRTIO_GPU_DEVICE_ID, &device) != 0) {
        gpu_last_error = 0xE010u;
        return -1;
    }

    command = pci_read_config16(&device, 0x04u);
    pci_write_config16(&device, 0x04u, (uint16_t)(command | 0x0006u));

    if (map_virtio_capability(&device, VIRTIO_PCI_CAP_COMMON,
                              &common_config, &common_length, 0) != 0 ||
        map_virtio_capability(&device, VIRTIO_PCI_CAP_NOTIFY,
                              &notify_region, &notify_length, &ignored_extra) != 0) {
        gpu_last_error = 0xE011u;
        return -1;
    }

    if (common_length < 56u || notify_length < 2u) {
        gpu_last_error = 0xE012u;
        return -1;
    }

    mmio_write8((uintptr_t)common_config + 20u, 0u);
    {
        uint32_t reset_wait;
        for (reset_wait = 0; reset_wait < 1000000u; ++reset_wait) {
            if (mmio_read8((uintptr_t)common_config + 20u) == 0u)
                break;
        }
        if (reset_wait == 1000000u) {
            gpu_last_error = 0xE01Cu;
            return -1;
        }
    }
    status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    mmio_write8((uintptr_t)common_config + 20u, status);

    mmio_write32((uintptr_t)common_config + 0u, 0u);
    features = mmio_read32((uintptr_t)common_config + 4u);
    mmio_write32((uintptr_t)common_config + 0u, 1u);
    features |= (uint64_t)mmio_read32((uintptr_t)common_config + 4u) << 32;
    if (!(features & (1ULL << VIRTIO_F_VERSION_1))) {
        gpu_set_error(0xE013u);
        return -1;
    }

    gpu_virgl_supported = (features & (1ULL << VIRTIO_GPU_F_VIRGL)) != 0;
    mmio_write32((uintptr_t)common_config + 8u, 0u);
    mmio_write32((uintptr_t)common_config + 12u, gpu_virgl_supported ? 1u : 0u);
    mmio_write32((uintptr_t)common_config + 8u, 1u);
    mmio_write32((uintptr_t)common_config + 12u, 1u);

    status |= VIRTIO_STATUS_FEATURES_OK;
    mmio_write8((uintptr_t)common_config + 20u, status);
    if (!(mmio_read8((uintptr_t)common_config + 20u) & VIRTIO_STATUS_FEATURES_OK)) {
        gpu_set_error(0xE014u);
        return -1;
    }

    notify_multiplier = ignored_extra;
    notify_region_length = notify_length;
    if (!notify_multiplier) {
        gpu_set_error(0xE01Bu);
        return -1;
    }
    if (virtio_setup_queue() != 0) {
        gpu_set_error(0xE015u);
        return -1;
    }
    if (virtio_setup_cursor_queue() != 0)
        cursor_notify_register = 0;

    status |= VIRTIO_STATUS_DRIVER_OK;
    mmio_write8((uintptr_t)common_config + 20u, status);

    if (gpu_query_display(&gpu_width, &gpu_height) != 0) {
        gpu_set_error(0xE016u);
        return -1;
    }

    if (gpu_width > 2048u || gpu_height > 2048u ||
        gpu_width > 0xFFFFFFFFu / 4u ||
        gpu_width * gpu_height > 0xFFFFFFFFu / 4u) {
        gpu_set_error(0xE017u);
        return -1;
    }

    byte_count = gpu_width * gpu_height * 4u;
    if (!byte_count || byte_count > GPU_MAX_BUFFER_BYTES ||
        byte_count > 0xFFFFFFFFu - (GPU_PAGE_SIZE - 1u)) {
        gpu_set_error(0xE018u);
        return -1;
    }

    backing_size = (byte_count + GPU_PAGE_SIZE - 1u) & ~(GPU_PAGE_SIZE - 1u);
    if (backing_size > 0xFFFFFFFFu - (GPU_PAGE_SIZE - 1u)) {
        gpu_set_error(0xE018u);
        return -1;
    }
    gpu_allocation = kmalloc(backing_size + GPU_PAGE_SIZE - 1u);
    if (!gpu_allocation) {
        gpu_set_error(0xE019u);
        return -1;
    }
    aligned_address = ((uintptr_t)gpu_allocation + GPU_PAGE_SIZE - 1u) &
                      ~(uintptr_t)(GPU_PAGE_SIZE - 1u);
    if (aligned_address > 0xFFFFFFFFu ||
        (aligned_address & (GPU_PAGE_SIZE - 1u)) ||
        aligned_address < 0x00100000u) {
        gpu_set_error(0xE019u);
        return -1;
    }
    gpu_pixels = (uint32_t *)aligned_address;
    gpu_buffer_size = backing_size;
    zero_bytes(gpu_pixels, gpu_buffer_size);

    if (gpu_create_scanout() != 0) {
        gpu_set_error(0xE01Au);
        return -1;
    }

    gpu_is_ready = 1;
    if (cursor_notify_register && gpu_create_hardware_cursor() == 0)
        gpu_cursor_is_ready = 1;
    if (pixels)
        *pixels = gpu_pixels;
    if (width)
        *width = gpu_width;
    if (height)
        *height = gpu_height;

    return virtio_gpu_present();
}

int virtio_gpu_present_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    gpu_transfer_2d_t transfer;
    gpu_resource_flush_t flush;

    if (!gpu_pixels || !common_config || !notify_register || !width || !height ||
        x >= gpu_width || y >= gpu_height)
        return -1;
    if (width > gpu_width - x) width = gpu_width - x;
    if (height > gpu_height - y) height = gpu_height - y;

    gpu_header_init(&transfer.header, GPU_CMD_TRANSFER_TO_HOST_2D);
    transfer.x = x;
    transfer.y = y;
    transfer.width = width;
    transfer.height = height;
    /* TRANSFER_TO_HOST_2D offset points to the first pixel of this rect
       in the linear guest backing store, not always to the start of it. */
    transfer.offset = ((uint64_t)y * (uint64_t)gpu_width + (uint64_t)x) * 4u;
    transfer.resource_id = GPU_RESOURCE_ID;
    transfer.padding = 0;
    if (gpu_send_simple(&transfer, sizeof(transfer), GPU_RESP_OK_NODATA) != 0)
        return -1;

    gpu_header_init(&flush.header, GPU_CMD_RESOURCE_FLUSH);
    flush.x = x;
    flush.y = y;
    flush.width = width;
    flush.height = height;
    flush.resource_id = GPU_RESOURCE_ID;
    flush.padding = 0;
    return gpu_send_simple(&flush, sizeof(flush), GPU_RESP_OK_NODATA);
}

int virtio_gpu_flush_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    gpu_resource_flush_t flush;
    if (!gpu_is_ready || !width || !height || x >= gpu_width || y >= gpu_height) return -1;
    if (width > gpu_width - x) width = gpu_width - x;
    if (height > gpu_height - y) height = gpu_height - y;
    gpu_header_init(&flush.header, GPU_CMD_RESOURCE_FLUSH);
    flush.x = x; flush.y = y; flush.width = width; flush.height = height;
    flush.resource_id = GPU_RESOURCE_ID; flush.padding = 0;
    return gpu_send_simple(&flush, sizeof(flush), GPU_RESP_OK_NODATA);
}

int virtio_gpu_present(void)
{
    return virtio_gpu_present_rect(0u, 0u, gpu_width, gpu_height);
}

int virtio_gpu_cursor_ready(void)
{
    return gpu_cursor_is_ready;
}

int virtio_gpu_cursor_move(int x, int y)
{
    gpu_update_cursor_t request;
    if (!gpu_cursor_is_ready) return -1;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if ((uint32_t)x >= gpu_width) x = (int)gpu_width - 1;
    if ((uint32_t)y >= gpu_height) y = (int)gpu_height - 1;
    zero_bytes(&request, sizeof(request));
    gpu_header_init(&request.header, GPU_CMD_MOVE_CURSOR);
    request.position.x = (uint32_t)x;
    request.position.y = (uint32_t)y;
    request.position.scanout_id = 0;
    return virtio_submit_cursor_command(&request, sizeof(request));
}

int virtio_gpu_virgl_supported(void)
{
    return gpu_is_ready && gpu_virgl_supported;
}

int virtio_gpu_virgl_query_capset(uint32_t capset_index, uint32_t *capset_id,
                                  uint32_t *max_version, uint32_t *max_size)
{
    gpu_get_capset_info_t request;
    gpu_capset_info_response_t response;

    if (!virtio_gpu_virgl_supported())
        return -1;
    gpu_header_init(&request.header, GPU_CMD_GET_CAPSET_INFO);
    request.capset_index = capset_index;
    if (virtio_submit_command(&request, sizeof(request), &response, sizeof(response)) != 0 ||
        response.header.type != 0x1102u)
        return -1;
    if (capset_id) *capset_id = response.capset_id;
    if (max_version) *max_version = response.max_version;
    if (max_size) *max_size = response.max_size;
    return 0;
}

int virtio_gpu_virgl_get_capset(uint32_t capset_id, uint32_t version,
                                void *buffer, uint32_t capacity, uint32_t *actual_size)
{
    gpu_get_capset_t request;
    static uint8_t response[8192] ALIGNED(16);
    uint32_t response_size;

    if (!virtio_gpu_virgl_supported() || !buffer || capacity == 0u)
        return -1;
    if (capacity > sizeof(response) - sizeof(gpu_ctrl_header_t))
        return -1;
    response_size = capacity + sizeof(gpu_ctrl_header_t);
    gpu_header_init(&request.header, GPU_CMD_GET_CAPSET);
    request.capset_id = capset_id;
    request.version = version;
    if (virtio_submit_command(&request, sizeof(request), response, response_size) != 0 ||
        *(uint32_t *)response != 0x1103u)
        return -1;
    {
        uint32_t bytes = response_size - sizeof(gpu_ctrl_header_t);
        const uint8_t *src = response + sizeof(gpu_ctrl_header_t);
        uint8_t *dst = (uint8_t *)buffer;
        for (uint32_t i = 0; i < bytes; ++i) dst[i] = src[i];
        if (actual_size) *actual_size = bytes;
    }
    return 0;
}

int virtio_gpu_virgl_context_create(uint32_t context_id, const char *name)
{
    gpu_context_create_t request;
    gpu_ctrl_header_t response;
    uint32_t length = 0;

    if (!virtio_gpu_virgl_supported() || !context_id || !name)
        return -1;
    while (name[length] && length < sizeof(request.debug_name) - 1u) ++length;
    zero_bytes(&request, sizeof(request));
    gpu_header_init(&request.header, GPU_CMD_CTX_CREATE);
    request.header.context_id = context_id;
    request.name_length = length;
    request.context_init = GPU_CAPSET_VIRGL;
    for (uint32_t i = 0; i < length; ++i) request.debug_name[i] = name[i];
    if (virtio_submit_command(&request, sizeof(request), &response, sizeof(response)) != 0 ||
        response.type != GPU_RESP_OK_NODATA)
        return -1;
    gpu_virgl_context_ready = 1;
    return 0;
}

int virtio_gpu_virgl_context_destroy(uint32_t context_id)
{
    gpu_ctrl_header_t request;
    gpu_ctrl_header_t response;
    if (!gpu_virgl_context_ready || !context_id) return -1;
    gpu_header_init(&request, GPU_CMD_CTX_DESTROY);
    request.context_id = context_id;
    if (virtio_submit_command(&request, sizeof(request), &response, sizeof(response)) != 0 ||
        response.type != GPU_RESP_OK_NODATA) return -1;
    gpu_virgl_context_ready = 0;
    return 0;
}

int virtio_gpu_virgl_create_resource(uint32_t resource_id, uint32_t target,
                                     uint32_t format, uint32_t bind,
                                     uint32_t width, uint32_t height, uint32_t depth)
{
    gpu_resource_create_3d_t request;
    gpu_ctrl_header_t response;
    if (!gpu_virgl_context_ready || !resource_id || !width || !height || !depth) return -1;
    zero_bytes(&request, sizeof(request));
    gpu_header_init(&request.header, GPU_CMD_RESOURCE_CREATE_3D);
    request.resource_id = resource_id;
    request.target = target;
    request.format = format;
    request.bind = bind;
    request.width = width;
    request.height = height;
    request.depth = depth;
    request.array_size = 1u;
    request.last_level = 0u;
    request.samples = 0u;
    if (virtio_submit_command(&request, sizeof(request), &response, sizeof(response)) != 0 ||
        response.type != GPU_RESP_OK_NODATA) return -1;
    return 0;
}

int virtio_gpu_virgl_attach_resource(uint32_t context_id, uint32_t resource_id)
{
    gpu_context_resource_t request;
    gpu_ctrl_header_t response;
    if (!gpu_virgl_context_ready || !context_id || !resource_id) return -1;
    gpu_header_init(&request.header, GPU_CMD_CTX_ATTACH_RESOURCE);
    request.header.context_id = context_id;
    request.resource_id = resource_id;
    request.padding = 0;
    if (virtio_submit_command(&request, sizeof(request), &response, sizeof(response)) != 0 ||
        response.type != GPU_RESP_OK_NODATA) return -1;
    return 0;
}

int virtio_gpu_virgl_submit(uint32_t context_id, const uint32_t *dwords, uint32_t count)
{
    gpu_submit_3d_t *request = (gpu_submit_3d_t *)virgl_submit_request;
    gpu_ctrl_header_t response;
    uint32_t bytes;

    if (!gpu_virgl_context_ready || !context_id || !dwords || !count || count > 4000u)
        return -1;
    bytes = count * 4u;
    zero_bytes(virgl_submit_request, sizeof(virgl_submit_request));
    gpu_header_init(&request->header, GPU_CMD_SUBMIT_3D);
    request->header.context_id = context_id;
    request->size = bytes;
    for (uint32_t i = 0; i < count; ++i)
        ((uint32_t *)(virgl_submit_request + sizeof(*request)))[i] = dwords[i];
    if (virtio_submit_command(virgl_submit_request, sizeof(*request) + bytes,
                              &response, sizeof(response)) != 0 ||
        response.type != GPU_RESP_OK_NODATA) return -1;
    return 0;
}

int virtio_gpu_ready(void)
{
    return gpu_is_ready;
}

uint32_t virtio_gpu_last_error(void)
{
    return gpu_last_error;
}

uint32_t virtio_gpu_last_command(void)
{
    return gpu_last_command;
}

uint32_t virtio_gpu_last_response_length(void)
{
    return gpu_last_response_length;
}
