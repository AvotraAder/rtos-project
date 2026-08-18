#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include "mutex.h"

#define QUEUE_SIZE 8

typedef struct {
    uint32_t data[QUEUE_SIZE];
    int      head;
    int      tail;
    int      count;
    mutex_t  lock;
} queue_t;

void queue_init(queue_t* q);
bool queue_push(queue_t* q, uint32_t val);
bool queue_pop(queue_t* q, uint32_t* val);

#endif /* QUEUE_H */
