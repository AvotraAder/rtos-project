#include "queue.h"

void queue_init(queue_t* q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    mutex_init(&q->lock);
}

bool queue_push(queue_t* q, uint32_t val) {
    mutex_lock(&q->lock);
    if (q->count >= QUEUE_SIZE) {
        mutex_unlock(&q->lock);
        return false;
    }
    q->data[q->tail] = val;
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    q->count++;
    mutex_unlock(&q->lock);
    return true;
}

bool queue_pop(queue_t* q, uint32_t* val) {
    mutex_lock(&q->lock);
    if (q->count == 0) {
        mutex_unlock(&q->lock);
        return false;
    }
    *val = q->data[q->head];
    q->head = (q->head + 1) % QUEUE_SIZE;
    q->count--;
    mutex_unlock(&q->lock);
    return true;
}
