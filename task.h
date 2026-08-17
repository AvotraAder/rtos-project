#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define MAX_TASKS 8
#define STACK_SIZE 4096
#define STACK_MAGIC 0xDEADC0DEu
#define AGING_STEP_TICKS 20
#define AGING_MAX_BOOST 3

typedef enum {
    TASK_READY,
    TASK_SLEEPING,
    TASK_BLOCKED,
    TASK_UNUSED
} task_state_t;

typedef struct task {
    uint32_t esp;
    uint32_t id;
    uint32_t priority;
    uint32_t wait_ticks;
    task_state_t state;
    uint32_t sleep_ticks;
    int used;
    uint32_t stack_magic_start; /* canary, doit rester à STACK_MAGIC */
    uint8_t stack[STACK_SIZE];
    struct task* next;
    struct task* wait_next; /* chaînage sur une liste d'attente (mutex/queue/sémaphore) */
} task_t;

void init_tasking(void);
int  create_task(void (*entry)(void), uint32_t priority); /* retourne l'id, ou -1 si le pool est plein */
int  task_kill(uint32_t id);
int  task_set_priority(uint32_t id, uint32_t new_priority);
uint32_t schedule(uint32_t current_esp);
void task_sleep(uint32_t ticks);
void task_block(task_t** wait_list_head);
void task_unblock_all(task_t** wait_list_head);
task_t* get_current_task(void);
task_t* task_find(uint32_t id);

#endif
