#include "task.h"

static task_t main_task;
static task_t* current_task = &main_task;
static uint32_t next_pid = 1;

void init_tasking(void) {
    main_task.id = 0;
    main_task.priority = 1;
    main_task.esp = 0;
    main_task.state = TASK_READY;
    main_task.sleep_ticks = 0;
    main_task.next = &main_task;
    current_task = &main_task;
}

void create_task(void (*entry)(void), uint32_t priority) {
    static task_t task_pool[5];
    static int pool_index = 0;

    task_t* new_task = &task_pool[pool_index++];
    new_task->id = next_pid++;
    new_task->priority = priority;
    new_task->state = TASK_READY;
    new_task->sleep_ticks = 0;

    uint32_t* stack_ptr = (uint32_t*)(new_task->stack + 4096);

    *(--stack_ptr) = 0x0202;          /* EFLAGS */
    *(--stack_ptr) = 0x08;            /* CS */
    *(--stack_ptr) = (uint32_t)entry; /* EIP */

    *(--stack_ptr) = 0;               /* Code d'erreur */
    *(--stack_ptr) = 32;              /* Numéro d'interruption */

    for (int i = 0; i < 8; i++) {
        *(--stack_ptr) = 0;           /* Pusha */
    }

    *(--stack_ptr) = 0x10;            /* DS */

    new_task->esp = (uint32_t)stack_ptr;

    new_task->next = current_task->next;
    current_task->next = new_task;
}

void task_sleep(uint32_t ticks) {
    if (ticks == 0) return;

    current_task->sleep_ticks = ticks;
    current_task->state = TASK_SLEEPING;

    __asm__ __volatile__ ("int $32");
}

uint32_t schedule(uint32_t current_esp) {
    if (!current_task) return current_esp;

    current_task->esp = current_esp;

    /* Mise à jour du sommeil */
    task_t* t = current_task;
    do {
        if (t->state == TASK_SLEEPING) {
            if (t->sleep_ticks > 0) {
                t->sleep_ticks--;
            }
            if (t->sleep_ticks == 0) {
                t->state = TASK_READY;
            }
        }
        t = t->next;
    } while (t != current_task);

    /* Sélection de la tâche PRÊTE de plus HAUTE PRIORITÉ */
    task_t* best_task = 0;
    uint32_t highest_prio = 0;

    t = current_task->next;
    do {
        if (t->state == TASK_READY && t->priority > highest_prio) {
            highest_prio = t->priority;
            best_task = t;
        }
        t = t->next;
    } while (t != current_task->next);

    if (best_task) {
        current_task = best_task;
    }

    return current_task->esp;
}

task_t* get_current_task(void) {
    return current_task;
}
