#ifndef SEMA_H
#define SEMA_H

#include <stdint.h>
#include "task.h"

typedef struct {
    volatile int32_t count;
    task_t* wait_list;
} sema_t;

void sema_init(sema_t* s, int32_t initial_count);
void sema_wait(sema_t* s);
void sema_signal(sema_t* s);

#endif
