#ifndef KMALLOC_H
#define KMALLOC_H

#include <stdint.h>

extern uint8_t kernel_end;

static uint8_t* heap_ptr = &kernel_end;

void* kmalloc(uint32_t size);
void kfree(void* ptr);

#endif