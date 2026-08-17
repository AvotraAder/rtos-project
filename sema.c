#include "sema.h"

void sema_init(sema_t* s, int32_t initial_count) {
    s->count = initial_count;
    s->wait_list = 0;
}

void sema_wait(sema_t* s) {
    while (1) {
        __asm__ __volatile__("cli");
        if (s->count > 0) {
            s->count--;
            __asm__ __volatile__("sti");
            return;
        }
        __asm__ __volatile__("sti");
        task_block(&s->wait_list);
    }
}

void sema_signal(sema_t* s) {
    __asm__ __volatile__("cli");
    s->count++;
    __asm__ __volatile__("sti");
    task_unblock_all(&s->wait_list);
}
