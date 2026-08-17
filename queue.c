#include "queue.h"

void queue_init(queue_t* q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->wait_producers = 0;
    q->wait_consumers = 0;
}

bool queue_push(queue_t* q, uint32_t val) {
    __asm__ __volatile__("cli");
    if (q->count >= QUEUE_SIZE) {
        __asm__ __volatile__("sti");
        return false;
    }
    q->data[q->tail] = val;
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    q->count++;
    __asm__ __volatile__("sti");
    task_unblock_all(&q->wait_consumers);
    return true;
}

bool queue_pop(queue_t* q, uint32_t* val) {
    __asm__ __volatile__("cli");
    if (q->count == 0) {
        __asm__ __volatile__("sti");
        return false;
    }
    *val = q->data[q->head];
    q->head = (q->head + 1) % QUEUE_SIZE;
    q->count--;
    __asm__ __volatile__("sti");
    task_unblock_all(&q->wait_producers);
    return true;
}

void queue_push_blocking(queue_t* q, uint32_t val) {
    while (!queue_push(q, val)) task_block(&q->wait_producers);
}

void queue_pop_blocking(queue_t* q, uint32_t* val) {
    while (!queue_pop(q, val)) task_block(&q->wait_consumers);
}
