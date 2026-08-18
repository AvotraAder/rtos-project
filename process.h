#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>

#define MAX_PROCESSES     16
#define MAX_PROC_NAME     32
#define PROC_STACK_SIZE   8192

/* ── États d'un processus ─────────────────────────────────────────── */
typedef enum {
    PROC_UNUSED   = 0,
    PROC_READY    = 1,
    PROC_RUNNING  = 2,
    PROC_SLEEPING = 3,
    PROC_BLOCKED  = 4,
    PROC_ZOMBIE   = 5
} proc_state_t;

/* ── Bloc de contrôle de processus (PCB) ──────────────────────────── */
typedef struct process {
    uint32_t     pid;
    uint32_t     ppid;
    char         name[MAX_PROC_NAME];
    proc_state_t state;
    uint32_t     priority;
    uint32_t     esp;
    uint32_t     sleep_ticks;
    uint32_t     cpu_time;
    uint32_t     wake_tick;
    uint8_t      stack[PROC_STACK_SIZE];
    struct process* next;
} process_t;

/* ── API publique ─────────────────────────────────────────────────── */
void      process_init(void);
int       process_create(const char* name,
                         void (*entry)(void),
                         uint32_t priority,
                         uint32_t ppid);
void      process_exit(uint32_t pid);
process_t* process_get(uint32_t pid);
process_t* process_current(void);
uint32_t  process_schedule(uint32_t current_esp);
void      process_sleep(uint32_t ticks);
void      process_list(void (*print_fn)(const char*));
uint32_t  process_count(void);

#endif /* PROCESS_H */
