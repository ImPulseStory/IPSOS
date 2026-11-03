#include "../include/heap.h"
#include <stdint.h>

void* kmalloc(uint32_t size) {
    size = (size + 7) & ~7;
    void* result = heap_ptr;
    heap_ptr += size;
    return result;
}

void kfree(void* ptr) {
    
}