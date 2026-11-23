#include "../include/vbe.h"
#include "../include/gfx.h"
#include <stdint.h>

#define VBE_INFO_FRAMEBUFFER 0x9000u
#define VBE_INFO_PITCH       0x9004u
#define VBE_INFO_WIDTH       0x9006u
#define VBE_INFO_HEIGHT      0x9008u
#define VBE_MAX_WIDTH        1024u
#define VBE_MAX_HEIGHT       768u
#define VBE_MAX_PITCH        16384u

static volatile uint32_t *framebuffer;
static uint32_t pitch;
static uint16_t screen_width;
static uint16_t screen_height;

void vbe_init(void)
{
    uint32_t address = *(volatile uint32_t *)(uintptr_t)VBE_INFO_FRAMEBUFFER;
    uint16_t reported_pitch = *(volatile uint16_t *)(uintptr_t)VBE_INFO_PITCH;
    uint16_t width = *(volatile uint16_t *)(uintptr_t)VBE_INFO_WIDTH;
    uint16_t height = *(volatile uint16_t *)(uintptr_t)VBE_INFO_HEIGHT;

    framebuffer = 0;
    pitch = 0;
    screen_width = 0;
    screen_height = 0;

    if (!address || address == 0xDEADBEEFu)
        return;
    if (!width || width > VBE_MAX_WIDTH || !height || height > VBE_MAX_HEIGHT)
        return;

    if (!reported_pitch)
        reported_pitch = (uint16_t)(width * sizeof(uint32_t));
    if (reported_pitch < (uint32_t)width * sizeof(uint32_t) ||
        reported_pitch > VBE_MAX_PITCH ||
        (reported_pitch & (sizeof(uint32_t) - 1u)) != 0)
        return;

    framebuffer = (volatile uint32_t *)(uintptr_t)address;
    pitch = reported_pitch;
    screen_width = width;
    screen_height = height;
}

int vbe_is_ready(void)
{
    return framebuffer != 0 && screen_width != 0 && screen_height != 0 && pitch != 0;
}

volatile uint32_t *vbe_get_framebuffer(void)
{
    return framebuffer;
}

uint32_t vbe_get_pitch(void)
{
    return pitch;
}

uint16_t vbe_get_width(void)
{
    if (gfx_ready())
        return (uint16_t)gfx_width();
    return screen_width;
}

uint16_t vbe_get_height(void)
{
    if (gfx_ready())
        return (uint16_t)gfx_height();
    return screen_height;
}
