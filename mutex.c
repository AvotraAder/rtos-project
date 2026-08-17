#include "mutex.h"
#include "task.h"

void mutex_init(mutex_t* mutex) {
    mutex->locked = 0;
    mutex->owner = 0;
}

void mutex_lock(mutex_t* mutex) {
    while (1) {
        __asm__ __volatile__("cli");
        if (mutex->locked == 0) {
            mutex->locked = 1;
            mutex->owner = get_current_task();
            __asm__ __volatile__("sti");
            break;
        }
        __asm__ __volatile__("sti");
        task_sleep(1);
    }
}

void mutex_unlock(mutex_t* mutex) {
    __asm__ __volatile__("cli");
    if (mutex->locked && mutex->owner == get_current_task()) {
        mutex->locked = 0;
        mutex->owner = 0;
    }
    __asm__ __volatile__("sti");
}
