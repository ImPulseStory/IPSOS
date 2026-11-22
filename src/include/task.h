#ifndef IPSOS_TASK_H
#define IPSOS_TASK_H

#include <stdint.h>

typedef void (*task_entry_t)(void *arg);

uint32_t task_create(task_entry_t entry, void *arg);
void task_yield(void);
void task_sleep(uint32_t milliseconds);
void task_exit(void);
uint32_t task_get_id(void);

#endif
