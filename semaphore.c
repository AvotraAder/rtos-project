#include "semaphore.h"
#include "task.h"

void sem_init(semaphore_t* sem, int32_t initial, uint32_t max)
{
    sem->count = initial;
    sem->max   = max;
}

/* ── P (wait / acquire) ───────────────────────────────────────────── */
void sem_wait(semaphore_t* sem)
{
    while (1) {
        __asm__ __volatile__ ("cli");

        if (sem->count > 0) {
            sem->count--;
            __asm__ __volatile__ ("sti");
            return;
        }

        __asm__ __volatile__ ("sti");
        task_sleep(1);   /* cède le CPU et réessaie après 1 tick */
    }
}

/* ── V (post / release) ───────────────────────────────────────────── */
void sem_post(semaphore_t* sem)
{
    __asm__ __volatile__ ("cli");

    if ((uint32_t)sem->count < sem->max)
        sem->count++;

    __asm__ __volatile__ ("sti");
}

/* ── Tentative non-bloquante ──────────────────────────────────────── */
int sem_trywait(semaphore_t* sem)
{
    int ret;
    __asm__ __volatile__ ("cli");

    if (sem->count > 0) {
        sem->count--;
        ret = 0;
    } else {
        ret = -1;
    }

    __asm__ __volatile__ ("sti");
    return ret;
}

/* ── Lecture de la valeur ─────────────────────────────────────────── */
int32_t sem_value(semaphore_t* sem)
{
    return sem->count;
}
