#ifndef TASK_H
#define TASK_H

#include <stdint.h>

/* États étendus */
typedef enum {
    TASK_READY    = 0,
    TASK_SLEEPING = 1,
    TASK_BLOCKED  = 2,
    TASK_ZOMBIE   = 3
} task_state_t;

typedef struct task {
    uint32_t     esp;
    uint32_t     id;
    uint32_t     priority;
    task_state_t state;
    uint32_t     sleep_ticks;
    uint32_t     cpu_ticks;     /* temps CPU cumulé (ticks) */
    uint8_t      stack[4096];
    struct task* next;
} task_t;

void    init_tasking(void);
void    create_task(void (*entry)(void), uint32_t priority);
uint32_t schedule(uint32_t current_esp);
void    task_sleep(uint32_t ticks);
void    task_yield(void);
task_t* get_current_task(void);
uint32_t task_count(void);

#endif /* TASK_H */
