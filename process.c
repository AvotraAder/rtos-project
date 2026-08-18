#include "process.h"
#include "timer.h"

/* ── Pool statique de PCB ─────────────────────────────────────────── */
static process_t  proc_pool[MAX_PROCESSES];
static process_t* proc_list_head = 0;
static process_t* current_proc   = 0;
static uint32_t   next_pid       = 1;

/* ─── Helpers internes ────────────────────────────────────────────── */
static const char* state_str(proc_state_t s)
{
    switch (s) {
        case PROC_UNUSED:   return "UNUSED  ";
        case PROC_READY:    return "READY   ";
        case PROC_RUNNING:  return "RUNNING ";
        case PROC_SLEEPING: return "SLEEPING";
        case PROC_BLOCKED:  return "BLOCKED ";
        case PROC_ZOMBIE:   return "ZOMBIE  ";
        default:            return "?       ";
    }
}

static void pstrcpy(char* d, const char* s, size_t max)
{
    size_t i;
    for (i = 0; i < max - 1 && s[i]; i++)
        d[i] = s[i];
    d[i] = '\0';
}

static void print_uint(void (*fn)(const char*), uint32_t n)
{
    char buf[16];
    int  i = 0;
    if (n == 0) { fn("0"); return; }
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    char out[16];
    for (int j = 0; j < i; j++) out[j] = buf[i - 1 - j];
    out[i] = '\0';
    fn(out);
}

/* ─── Initialisation ──────────────────────────────────────────────── */
void process_init(void)
{
    for (int i = 0; i < MAX_PROCESSES; i++) {
        proc_pool[i].state = PROC_UNUSED;
        proc_pool[i].pid   = 0;
    }

    /* Processus idle PID 0 */
    process_t* idle   = &proc_pool[0];
    idle->pid         = 0;
    idle->ppid        = 0;
    idle->priority    = 0;
    idle->state       = PROC_READY;
    idle->esp         = 0;
    idle->sleep_ticks = 0;
    idle->cpu_time    = 0;
    idle->wake_tick   = 0;
    idle->next        = idle;
    pstrcpy(idle->name, "idle", MAX_PROC_NAME);

    proc_list_head = idle;
    current_proc   = idle;
    next_pid       = 1;
}

/* ─── Création ────────────────────────────────────────────────────── */
int process_create(const char* name,
                   void (*entry)(void),
                   uint32_t priority,
                   uint32_t ppid)
{
    process_t* np = 0;
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (proc_pool[i].state == PROC_UNUSED) {
            np = &proc_pool[i];
            break;
        }
    }
    if (!np) return -1;

    np->pid        = next_pid++;
    np->ppid       = ppid;
    np->priority   = priority;
    np->state      = PROC_READY;
    np->sleep_ticks= 0;
    np->cpu_time   = 0;
    np->wake_tick  = 0;
    pstrcpy(np->name, name, MAX_PROC_NAME);

    /* Construction de la pile initiale */
    uint32_t* sp = (uint32_t*)(np->stack + PROC_STACK_SIZE);

    *(--sp) = 0x0202;           /* EFLAGS */
    *(--sp) = 0x08;             /* CS     */
    *(--sp) = (uint32_t)entry;  /* EIP    */
    *(--sp) = 0;                /* err_code */
    *(--sp) = 32;               /* int_no   */
    for (int i = 0; i < 8; i++) *(--sp) = 0;  /* pusha  */
    *(--sp) = 0x10;             /* DS     */

    np->esp = (uint32_t)sp;

    /* Insertion en fin de liste circulaire */
    process_t* t = proc_list_head;
    while (t->next != proc_list_head) t = t->next;
    t->next  = np;
    np->next = proc_list_head;

    return (int)np->pid;
}

/* ─── Fin de processus ────────────────────────────────────────────── */
void process_exit(uint32_t pid)
{
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_pool[i].pid == pid &&
            proc_pool[i].state != PROC_UNUSED) {
            proc_pool[i].state = PROC_ZOMBIE;
            return;
        }
    }
}

/* ─── Accesseurs ──────────────────────────────────────────────────── */
process_t* process_get(uint32_t pid)
{
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_pool[i].state != PROC_UNUSED &&
            proc_pool[i].pid   == pid)
            return &proc_pool[i];
    }
    return 0;
}

process_t* process_current(void)
{
    return current_proc;
}

uint32_t process_count(void)
{
    uint32_t cnt = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_pool[i].state != PROC_UNUSED &&
            proc_pool[i].state != PROC_ZOMBIE)
            cnt++;
    }
    return cnt;
}

/* ─── Ordonnanceur ────────────────────────────────────────────────── */
uint32_t process_schedule(uint32_t current_esp)
{
    if (!current_proc) return current_esp;

    current_proc->esp = current_esp;
    if (current_proc->state == PROC_RUNNING)
        current_proc->state = PROC_READY;

    uint32_t now = get_ticks();
    process_t* t = proc_list_head;
    do {
        if (t->state == PROC_SLEEPING && now >= t->wake_tick)
            t->state = PROC_READY;
        if (t->state == PROC_ZOMBIE)
            t->state = PROC_UNUSED;
        t->cpu_time++;
        t = t->next;
    } while (t != proc_list_head);

    process_t* best     = 0;
    uint32_t   best_pri = 0;
    t = current_proc->next;
    process_t* stop = t;
    do {
        if (t->state == PROC_READY && t->priority >= best_pri) {
            best_pri = t->priority;
            best     = t;
        }
        t = t->next;
    } while (t != stop);

    if (!best) best = &proc_pool[0];

    current_proc        = best;
    current_proc->state = PROC_RUNNING;

    return current_proc->esp;
}

/* ─── Sleep ───────────────────────────────────────────────────────── */
void process_sleep(uint32_t ticks)
{
    if (ticks == 0) return;
    current_proc->sleep_ticks = ticks;
    current_proc->wake_tick   = get_ticks() + ticks;
    current_proc->state       = PROC_SLEEPING;
    __asm__ __volatile__ ("int $32");
}

/* ─── Listage ─────────────────────────────────────────────────────── */
void process_list(void (*print_fn)(const char*))
{
    print_fn(" PID  PPID  PRI  CPU    STATE     NAME\n");
    print_fn(" ---  ----  ---  -----  --------  --------\n");

    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* p = &proc_pool[i];
        if (p->state == PROC_UNUSED) continue;

        print_fn(" ");
        print_uint(print_fn, p->pid);   print_fn("     ");
        print_uint(print_fn, p->ppid);  print_fn("     ");
        print_uint(print_fn, p->priority); print_fn("    ");
        print_uint(print_fn, p->cpu_time); print_fn("    ");
        print_fn(state_str(p->state));  print_fn("  ");
        print_fn(p->name);
        print_fn("\n");
    }
}
