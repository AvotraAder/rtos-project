#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include "task.h"

#define QUEUE_SIZE 8

typedef struct {
    uint32_t data[QUEUE_SIZE];
    int head;
    int tail;
    int count;
    task_t* wait_producers;
    task_t* wait_consumers;
} queue_t;

void queue_init(queue_t* q);
bool queue_push(queue_t* q, uint32_t val);          /* non bloquant */
bool queue_pop(queue_t* q, uint32_t* val);           /* non bloquant */
void queue_push_blocking(queue_t* q, uint32_t val);  /* bloque tant que pleine */
void queue_pop_blocking(queue_t* q, uint32_t* val);  /* bloque tant que vide */

#endif
