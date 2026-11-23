#ifndef VBE_H
#define VBE_H

#include <stdint.h>

void vbe_init(void);
int vbe_is_ready(void);
volatile uint32_t *vbe_get_framebuffer(void);
uint32_t vbe_get_pitch(void);
uint16_t vbe_get_width(void);
uint16_t vbe_get_height(void);

#endif
