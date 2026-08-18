#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <stdint.h>

/*
 * Sémaphore compteur.
 * - sem_wait() : décrémente ; bloque (spin + yield) si valeur <= 0
 * - sem_post() : incrémente (signal)
 */
typedef struct {
    volatile int32_t count;
    uint32_t         max;
} semaphore_t;

void sem_init(semaphore_t* sem, int32_t initial, uint32_t max);
void sem_wait(semaphore_t* sem);   /* P / acquire */
void sem_post(semaphore_t* sem);   /* V / release */
int  sem_trywait(semaphore_t* sem);/* non-bloquant : 0=ok, -1=occupé */
int32_t sem_value(semaphore_t* sem);

#endif /* SEMAPHORE_H */
