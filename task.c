#include "task.h"
#include "panic.h"

static task_t task_pool[MAX_TASKS];
static task_t idle_task_slot;
static task_t* current_task = 0;
static uint32_t next_pid = 1;

static void idle_entry(void) {
    while (1) { __asm__ __volatile__ ("hlt"); }
}

static void setup_stack(task_t* t, void (*entry)(void)) {
    uint32_t* stack_ptr = (uint32_t*)(t->stack + STACK_SIZE);
    *(--stack_ptr) = 0x0202;
    *(--stack_ptr) = 0x08;
    *(--stack_ptr) = (uint32_t)entry;
    *(--stack_ptr) = 0;
    *(--stack_ptr) = 32;
    for (int i = 0; i < 8; i++) *(--stack_ptr) = 0;
    *(--stack_ptr) = 0x10;
    t->esp = (uint32_t)stack_ptr;
}

void init_tasking(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        task_pool[i].used = 0;
        task_pool[i].state = TASK_UNUSED;
    }

    /* tâche idle : garantit toujours au moins une tâche READY,
       ce qui élimine le deadlock si toutes les autres tâches dorment/bloquent */
    idle_task_slot.id = 0;
    idle_task_slot.priority = 0;
    idle_task_slot.wait_ticks = 0;
    idle_task_slot.state = TASK_READY;
    idle_task_slot.sleep_ticks = 0;
    idle_task_slot.used = 1;
    idle_task_slot.stack_magic_start = STACK_MAGIC;
    idle_task_slot.wait_next = 0;
    setup_stack(&idle_task_slot, idle_entry);
    idle_task_slot.next = &idle_task_slot;

    current_task = &idle_task_slot;
}

int create_task(void (*entry)(void), uint32_t priority) {
    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (!task_pool[i].used) { slot = i; break; }
    }
    if (slot == -1) return -1; /* pool plein : avant, ça débordait le tableau silencieusement */

    task_t* new_task = &task_pool[slot];
    new_task->id = next_pid++;
    new_task->priority = priority;
    new_task->wait_ticks = 0;
    new_task->state = TASK_READY;
    new_task->sleep_ticks = 0;
    new_task->used = 1;
    new_task->stack_magic_start = STACK_MAGIC;
    new_task->wait_next = 0;
    setup_stack(new_task, entry);

    __asm__ __volatile__("cli");
    new_task->next = current_task->next;
    current_task->next = new_task;
    __asm__ __volatile__("sti");

    return (int)new_task->id;
}

int task_kill(uint32_t id) {
    if (id == 0) return -1;                  /* impossible de tuer idle */
    if (current_task->id == id) return -1;   /* impossible de se tuer soi-même */

    __asm__ __volatile__("cli");
    task_t* prev = current_task;
    task_t* t = current_task->next;
    do {
        if (t->id == id && t->used) {
            prev->next = t->next;
            t->used = 0;
            t->state = TASK_UNUSED;
            __asm__ __volatile__("sti");
            return 0;
        }
        prev = t;
        t = t->next;
    } while (t != current_task);
    __asm__ __volatile__("sti");
    return -1;
}

int task_set_priority(uint32_t id, uint32_t new_priority) {
    task_t* t = task_find(id);
    if (!t) return -1;
    t->priority = new_priority;
    return 0;
}

task_t* task_find(uint32_t id) {
    task_t* t = current_task;
    do {
        if (t->used && t->id == id) return t;
        t = t->next;
    } while (t != current_task);
    return 0;
}

void task_sleep(uint32_t ticks) {
    if (ticks == 0) return;
    current_task->sleep_ticks = ticks;
    current_task->state = TASK_SLEEPING;
    __asm__ __volatile__ ("int $32");
}

void task_block(task_t** wait_list_head) {
    __asm__ __volatile__("cli");
    current_task->state = TASK_BLOCKED;
    current_task->wait_next = *wait_list_head;
    *wait_list_head = current_task;
    __asm__ __volatile__("sti");
    __asm__ __volatile__ ("int $32");
}

void task_unblock_all(task_t** wait_list_head) {
    __asm__ __volatile__("cli");
    task_t* t = *wait_list_head;
    while (t) {
        task_t* nxt = t->wait_next;
        t->state = TASK_READY;
        t->wait_next = 0;
        t = nxt;
    }
    *wait_list_head = 0;
    __asm__ __volatile__("sti");
}

uint32_t schedule(uint32_t current_esp) {
    if (!current_task) return current_esp;

    if (current_task->stack_magic_start != STACK_MAGIC) {
        panic_msg("Stack overflow detected on task", current_task->id);
    }

    current_task->esp = current_esp;

    task_t* t = current_task;
    do {
        if (t->used) {
            if (t->state == TASK_SLEEPING) {
                if (t->sleep_ticks > 0) t->sleep_ticks--;
                if (t->sleep_ticks == 0) t->state = TASK_READY;
            } else if (t->state == TASK_READY) {
                t->wait_ticks++;
            }
        }
        t = t->next;
    } while (t != current_task);

    /* priorité "effective" = priorité de base + bonus d'aging,
       pour éviter la famine des tâches basse priorité */
    task_t* best_task = 0;
    uint32_t best_effective = 0;
    t = current_task->next;
    do {
        if (t->state == TASK_READY) {
            uint32_t boost = t->wait_ticks / AGING_STEP_TICKS;
            if (boost > AGING_MAX_BOOST) boost = AGING_MAX_BOOST;
            uint32_t effective = t->priority + boost;
            if (!best_task || effective > best_effective) {
                best_effective = effective;
                best_task = t;
            }
        }
        t = t->next;
    } while (t != current_task->next);

    if (!best_task) {
        best_task = (current_task->state == TASK_READY) ? current_task : &idle_task_slot;
    }

    current_task = best_task;
    current_task->wait_ticks = 0;

    return current_task->esp;
}

task_t* get_current_task(void) {
    return current_task;
}
