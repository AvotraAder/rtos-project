#include "task.h"

/* ── Données privées ─────────────────────────────────────────────── */
static task_t  main_task;
static task_t* current_task = &main_task;
static uint32_t next_pid    = 1;
static uint32_t total_tasks = 1;

#define TASK_POOL_SIZE 8
static task_t task_pool[TASK_POOL_SIZE];
static int    pool_index = 0;

/* ── Initialisation ──────────────────────────────────────────────── */
void init_tasking(void)
{
    main_task.id          = 0;
    main_task.priority    = 1;
    main_task.esp         = 0;
    main_task.state       = TASK_READY;
    main_task.sleep_ticks = 0;
    main_task.cpu_ticks   = 0;
    main_task.next        = &main_task;
    current_task          = &main_task;
    total_tasks           = 1;
    pool_index            = 0;
}

/* ── Création d'une tâche ────────────────────────────────────────── */
void create_task(void (*entry)(void), uint32_t priority)
{
    if (pool_index >= TASK_POOL_SIZE) return;

    task_t* nt = &task_pool[pool_index++];
    nt->id          = next_pid++;
    nt->priority    = priority;
    nt->state       = TASK_READY;
    nt->sleep_ticks = 0;
    nt->cpu_ticks   = 0;

    /* Construction de la pile initiale */
    uint32_t* sp = (uint32_t*)(nt->stack + 4096);

    /* iret frame */
    *(--sp) = 0x0202;           /* EFLAGS */
    *(--sp) = 0x08;             /* CS     */
    *(--sp) = (uint32_t)entry;  /* EIP    */

    /* err_code + int_no (consommés par add esp,8) */
    *(--sp) = 0;
    *(--sp) = 32;

    /* pusha : 8 registres */
    for (int i = 0; i < 8; i++) *(--sp) = 0;

    /* DS */
    *(--sp) = 0x10;

    nt->esp = (uint32_t)sp;

    /* Insertion dans la liste circulaire */
    nt->next = current_task->next;
    current_task->next = nt;

    total_tasks++;
}

/* ── Ordonnanceur préemptif ──────────────────────────────────────── */
uint32_t schedule(uint32_t current_esp)
{
    if (!current_task) return current_esp;

    current_task->esp = current_esp;

    /* Décrémente les compteurs de sommeil */
    task_t* t = current_task;
    do {
        if (t->state == TASK_SLEEPING) {
            if (t->sleep_ticks > 0) t->sleep_ticks--;
            if (t->sleep_ticks == 0) t->state = TASK_READY;
        }
        t = t->next;
    } while (t != current_task);

    /* Sélection par priorité la plus haute */
    task_t*  best      = 0;
    uint32_t best_prio = 0;

    t = current_task->next;
    do {
        if (t->state == TASK_READY && t->priority > best_prio) {
            best_prio = t->priority;
            best      = t;
        }
        t = t->next;
    } while (t != current_task->next);

    if (best) {
        current_task = best;
    }

    current_task->cpu_ticks++;
    return current_task->esp;
}

/* ── Sleep ───────────────────────────────────────────────────────── */
void task_sleep(uint32_t ticks)
{
    if (ticks == 0) return;
    current_task->sleep_ticks = ticks;
    current_task->state       = TASK_SLEEPING;
    __asm__ __volatile__ ("int $32");
}

/* ── Yield volontaire ────────────────────────────────────────────── */
void task_yield(void)
{
    __asm__ __volatile__ ("int $32");
}

/* ── Accesseurs ──────────────────────────────────────────────────── */
task_t* get_current_task(void)
{
    return current_task;
}

uint32_t task_count(void)
{
    return total_tasks;
}
