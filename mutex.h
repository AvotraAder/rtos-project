#ifndef MUTEX_H
#define MUTEX_H

#include <stdint.h>
#include "task.h"

typedef struct {
    volatile uint32_t locked;
    task_t* owner;
} mutex_t;

void mutex_init(mutex_t* mutex);
void mutex_lock(mutex_t* mutex);
void mutex_unlock(mutex_t* mutex);

#endif
