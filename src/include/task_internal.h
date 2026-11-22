#ifndef IPSOS_TASK_INTERNAL_H
#define IPSOS_TASK_INTERNAL_H

#include "task.h"

void task_init(void);
uint32_t task_create_group(task_entry_t entry, void *arg, uint32_t *group_id);
void task_wait_group(uint32_t group_id);
void task_exit_group(void);
uint32_t *task_schedule(uint32_t *saved_esp, uint32_t from_timer);
void ips_process_exit(int status);
void ips_process_abort(void);

#endif
