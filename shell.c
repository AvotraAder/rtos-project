/*
================================================================================
FICHIER: shell.c
VERSION: 3 - vmmap fonctionnel + commande shutdown
================================================================================
*/

#include "shell.h"
#include "timer.h"
#include "task.h"
#include "heap.h"
#include "vfs.h"
#include "syscall.h"
#include "semaphore.h"
#include "process.h"
#include "logging.h"
#include "error.h"
#include "paging.h"
#include "signal.h"
#include "ring.h"
#include <stdint.h>
#include <stddef.h>

extern void terminal_putchar(char c);
extern void terminal_write_at(const char* str, int row, int col);
extern void terminal_clear(void);
extern void draw_header(void);
extern void terminal_scroll_to_bottom(void);
extern int  terminal_get_view_offset(void);

#define MAX_BUFFER  128
#define MAX_ARGS    8
#define ARG_LEN     32

#define HISTORY_SIZE 20
static char command_history[HISTORY_SIZE][MAX_BUFFER];
static int  history_count = 0;
static int  history_index = -1;
static char saved_buffer[MAX_BUFFER];
static int  saved_buf_idx = 0;

static char  buffer[MAX_BUFFER];
static int   buf_idx        = 0;
static void* test_alloc_ptr = 0;

/* ──────────────────────────────────────────────────────────────────
   String utility functions
   ────────────────────────────────────────────────────────────────── */

static int sh_strcmp(const char* a, const char* b)
{
    while (*a && (*a == *b)) { a++; b++; }
    return *(unsigned char*)a - *(unsigned char*)b;
}

static void sh_strcpy(char* dest, const char* src)
{
    while ((*dest++ = *src++));
}

static int sh_strlen(const char* s)
{
    int len = 0;
    while (s[len]) len++;
    return len;
}

static void print_str(const char* s)
{
    for (int i = 0; s[i]; i++) terminal_putchar(s[i]);
}

static void print_dec(uint32_t n)
{
    if (n == 0) { terminal_putchar('0'); return; }
    char buf[16]; 
    int i = 0;
    while (n > 0) { 
        buf[i++] = '0' + (n % 10); 
        n /= 10; 
    }
    for (int j = i - 1; j >= 0; j--) terminal_putchar(buf[j]);
}

static void print_hex(uint32_t n)
{
    const char* hx = "0123456789ABCDEF";
    print_str("0x");
    for (int i = 7; i >= 0; i--)
        terminal_putchar(hx[(n >> (i * 4)) & 0xF]);
}

static void print_prompt(void) { print_str("\nRTOS> "); }

static int split_args(char* cmd, char args[MAX_ARGS][ARG_LEN], int* argc)
{
    *argc = 0;
    int i = 0;
    while (cmd[i] == ' ') i++;
    while (cmd[i] && *argc < MAX_ARGS) {
        int j = 0;
        while (cmd[i] && cmd[i] != ' ' && j < ARG_LEN - 1)
            args[*argc][j++] = cmd[i++];
        args[*argc][j] = '\0';
        (*argc)++;
        while (cmd[i] == ' ') i++;
    }
    return *argc;
}

static void history_add(const char* cmd)
{
    if (sh_strlen(cmd) == 0) return;
    
    if (history_count > 0 && sh_strcmp(command_history[history_count - 1], cmd) == 0) {
        return;
    }
    
    if (history_count < HISTORY_SIZE) {
        sh_strcpy(command_history[history_count], cmd);
        history_count++;
    } else {
        for (int i = 0; i < HISTORY_SIZE - 1; i++) {
            sh_strcpy(command_history[i], command_history[i + 1]);
        }
        sh_strcpy(command_history[HISTORY_SIZE - 1], cmd);
    }
}

static void clear_input_line(void)
{
    while (buf_idx > 0) {
        terminal_putchar('\b');
        buf_idx--;
    }
}

static void display_buffer(void)
{
    for (int i = 0; i < buf_idx; i++) {
        terminal_putchar(buffer[i]);
    }
}

void shell_history_up(void)
{
    if (history_count == 0) return;
    
    if (history_index == -1) {
        sh_strcpy(saved_buffer, buffer);
        saved_buf_idx = buf_idx;
        history_index = history_count - 1;
    } else if (history_index > 0) {
        history_index--;
    } else {
        return;
    }
    
    clear_input_line();
    sh_strcpy(buffer, command_history[history_index]);
    buf_idx = sh_strlen(buffer);
    display_buffer();
}

void shell_history_down(void)
{
    if (history_index == -1) return;
    
    clear_input_line();
    
    if (history_index < history_count - 1) {
        history_index++;
        sh_strcpy(buffer, command_history[history_index]);
        buf_idx = sh_strlen(buffer);
    } else {
        history_index = -1;
        sh_strcpy(buffer, saved_buffer);
        buf_idx = saved_buf_idx;
    }
    
    display_buffer();
}

/* ──────────────────────────────────────────────────────────────────
   Low-level port I/O (needed for shutdown via ACPI/QEMU)
   ────────────────────────────────────────────────────────────────── */

static inline void sh_outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__ ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void sh_outw(uint16_t port, uint16_t val) {
    __asm__ __volatile__ ("outw %0, %1" : : "a"(val), "Nd"(port));
}

/* ──────────────────────────────────────────────────────────────────
   RTOS v3.1 new commands
   ────────────────────────────────────────────────────────────────── */

static void cmd_help(void)
{
    print_str("\n=== RTOS Shell v3.0 ===\n");
    print_str("\n  SYSTEM:\n");
    print_str("   help              - This help\n");
    print_str("   clear             - Clear the screen\n");
    print_str("   uptime            - Time since boot\n");
    print_str("   ticks             - Current PIT ticks\n");
    print_str("   reboot            - Reboot\n");
    print_str("   shutdown          - Power off the system\n");
    print_str("   sysinfo           - System information\n");
    
    print_str("\n  LOGGING:\n");
    print_str("   logs              - Show the log buffer\n");
    print_str("   loglevel <0-4>    - Set the log level\n");
    print_str("   logclear          - Clear the log buffer\n");
    
    print_str("\n  PAGING:\n");
    print_str("   pages             - Paging statistics\n");
    print_str("   vmmap             - Show virtual memory map\n");
    
    print_str("\n  SIGNALS & RINGS:\n");
    print_str("   ring              - Show current Ring\n");
    print_str("   siglist           - List signals\n");
    print_str("   testsig <pid>     - Send SIGTERM to a PID\n");
    
    print_str("\n  NAVIGATION:\n");
    print_str("   Page Up/Down      - Scroll up/down\n");
    print_str("   Up/Down Arrow     - Command history\n");
    
    print_str("\n  TASKS & PROCESSES:\n");
    print_str("   tasks             - List tasks (scheduler)\n");
    print_str("   ps                - List processes (PCB)\n");
    print_str("   spawn <name>      - Create a process\n");
    
    print_str("\n  MEMORY:\n");
    print_str("   mem               - Heap usage\n");
    print_str("   alloc [bytes]     - Allocate memory\n");
    print_str("   free              - Free last allocation\n");
    print_str("   hexdump <addr>    - Hex dump 64 bytes\n");
    
    print_str("\n  VFS FILES:\n");
    print_str("   ls                - List files\n");
    print_str("   cat <file>        - Show file content\n");
    print_str("   touch <file>      - Create an empty file\n");
    print_str("   write <f> <text>  - Write to a file\n");
    print_str("   rm <file>         - Remove a file\n");
    
    print_str("\n  MISC:\n");
    print_str("   echo <text>       - Print text\n");
    print_str("   calc <n> <op> <m> - Calculator (+ - * /)\n");
    print_str("   sem               - Semaphore demo\n");
    print_str("   sys               - Syscall test\n");
    print_str("   history           - Show command history\n");
}

static void cmd_history(void)
{
    print_str("\n=== Command History ===\n");
    if (history_count == 0) {
        print_str("  (empty)\n");
        return;
    }
    for (int i = 0; i < history_count; i++) {
        print_str("  ");
        print_dec((uint32_t)(i + 1));
        print_str(": ");
        print_str(command_history[i]);
        print_str("\n");
    }
}

static void cmd_uptime(void)
{
    uint32_t s = get_uptime_sec();
    uint32_t m = s / 60;
    uint32_t h = m / 60;
    print_str("\nUptime: ");
    print_dec(h); print_str("h ");
    print_dec(m % 60); print_str("m ");
    print_dec(s % 60); print_str("s  (");
    print_dec(get_ticks()); print_str(" ticks)");
}

static void cmd_sysinfo(void)
{
    print_str("\n=== RTOS System Info ===\n");
    print_str("Version: RTOS v3.0\n");
    print_str("Built: 2026-08-19\n");
    print_str("Tasks active: ");
    print_dec(task_count());
    print_str("\n");
    print_str("Processes: ");
    print_dec(process_count());
    print_str("\n");
    print_str("Free pages: ");
    print_dec(paging_get_free_pages());
    print_str("\n");
}

static void cmd_logs(void)
{
    log_dump(print_str);
}

static void cmd_loglevel(int argc, char args[MAX_ARGS][ARG_LEN])
{
    if (argc < 2) {
        print_str("\nUsage: loglevel <0-4>\n");
        print_str("  0=DEBUG, 1=INFO, 2=WARN, 3=ERROR, 4=CRITICAL\n");
        return;
    }
    
    int level = 0;
    for (int i = 0; args[1][i] >= '0' && args[1][i] <= '9'; i++) {
        level = level * 10 + (args[1][i] - '0');
    }
    
    if (level >= 0 && level <= 4) {
        log_set_level((log_level_t)level);
        print_str("\nLog level changed to ");
        print_dec(level);
    }
}

static void cmd_pages(void)
{
    print_str("\n=== Paging Statistics ===\n");
    print_str("Total pages: ");
    print_dec(1024 * 1024);
    print_str("\nUsed pages: ");
    print_dec(paging_get_used_pages());
    print_str("\nFree pages: ");
    print_dec(paging_get_free_pages());
    print_str("\n");
}

/* ── vmmap : carte mémoire virtuelle (NOUVEAU - fonctionnel) ──────── */
static void print_range(const char* label, uint32_t start, uint32_t end)
{
    print_str("  ");
    print_str(label);
    print_hex(start);
    print_str(" - ");
    print_hex(end);
    print_str("\n");
}

static void cmd_vmmap(void)
{
    print_str("\n=== Virtual Memory Map ===\n");

    print_range("Kernel image     : ", 0x00100000, 0x00400000);
    print_range("Kernel heap       : ", KERNEL_HEAP_BASE, KERNEL_HEAP_BASE + (4 * 1024 * 1024));
    print_range("Identity map      : ", 0x00000000, 0x04000000); /* 64 Mo, cf paging.c */
    print_range("VGA text buffer   : ", 0x000B8000, 0x000B8FA0);
    print_range("User code base    : ", USER_BASE, USER_HEAP_BASE);
    print_range("User heap base    : ", USER_HEAP_BASE, USER_STACK_TOP);
    print_range("User stack top    : ", USER_STACK_TOP, 0xFFFFFFFF);

    print_str("\nPage tables: ");
    print_dec(PAGE_TABLES_COUNT);
    print_str(" x ");
    print_dec(PAGES_PER_TABLE);
    print_str(" entries (");
    print_dec(PAGE_SIZE);
    print_str(" bytes/page)\n");

    print_str("Used pages : ");
    print_dec(paging_get_used_pages());
    print_str("   Free pages: ");
    print_dec(paging_get_free_pages());
    print_str("\n");
}

static void cmd_ring(void)
{
    uint32_t ring = get_current_ring();
    print_str("\nCurrent Ring: Ring ");
    print_dec(ring);
    if (ring == 0) {
        print_str(" (Kernel mode)\n");
    } else {
        print_str(" (User mode)\n");
    }
}

static void cmd_siglist(void)
{
    print_str("\n=== RTOS Signals ===\n");
    print_str("SIGHUP  (1)   - Hangup\n");
    print_str("SIGINT  (2)   - Interrupt (Ctrl+C)\n");
    print_str("SIGTERM (15)  - Termination\n");
    print_str("SIGKILL (9)   - Forced (non-blockable)\n");
    print_str("SIGUSR1 (10)  - User signal 1\n");
    print_str("SIGUSR2 (11)  - User signal 2\n");
}

static void cmd_testsig(int argc, char args[MAX_ARGS][ARG_LEN])
{
    if (argc < 2) {
        print_str("\nUsage: testsig <pid>\n");
        return;
    }
    
    uint32_t pid = 0;
    for (int i = 0; args[1][i] >= '0' && args[1][i] <= '9'; i++) {
        pid = pid * 10 + (args[1][i] - '0');
    }
    
    error_t err = kill(pid, SIGTERM);
    print_str("\nSIGTERM signal sent to PID ");
    print_dec(pid);
    print_str(": ");
    print_str(error_to_string(err));
}

static void cmd_tasks(void)
{
    print_str("\n ID   PRI  CPU   STATE\n");
    print_str(" ---  ---  ---   ---------\n");
    task_t* start = get_current_task();
    task_t* cur   = start;
    do {
        print_str(" ");
        print_dec(cur->id);        print_str("    ");
        print_dec(cur->priority);  print_str("    ");
        print_dec(cur->cpu_ticks); print_str("   ");
        switch (cur->state) {
            case TASK_READY:    print_str("READY\n"); break;
            case TASK_SLEEPING: 
                print_str("SLEEP(");
                print_dec(cur->sleep_ticks);
                print_str(")\n"); 
                break;
            case TASK_BLOCKED:  print_str("BLOCKED\n"); break;
            case TASK_ZOMBIE:   print_str("ZOMBIE\n"); break;
        }
        cur = cur->next;
    } while (cur != start);
}

static void cmd_ps(void)
{
    print_str("\n");
    process_list(print_str);
}

static void cmd_mem(void)
{
    size_t used  = heap_get_used();
    size_t avail = heap_get_free();
    size_t total = used + avail;
    print_str("\nHeap total: ");
    print_dec((uint32_t)(total / 1024)); print_str(" KB\n");
    print_str("  Used     : "); print_dec((uint32_t)used);  print_str(" bytes\n");
    print_str("  Free     : "); print_dec((uint32_t)avail); print_str(" bytes\n");
    uint32_t pct = total ? (uint32_t)(used * 40 / total) : 0;
    print_str("  [");
    for (uint32_t i = 0; i < 40; i++) {
        terminal_putchar(i < pct ? '#' : '.');
    }
    print_str("] ");
    print_dec(total ? (uint32_t)(used * 100 / total) : 0);
    print_str("%");
}

static void cmd_alloc(int argc, char args[MAX_ARGS][ARG_LEN])
{
    if (test_alloc_ptr) {
        print_str("\nAlready allocated! Run 'free' first."); 
        return;
    }
    uint32_t sz = 1024;
    if (argc >= 2) {
        sz = 0;
        for (int i = 0; args[1][i] >= '0' && args[1][i] <= '9'; i++) {
            sz = sz * 10 + (args[1][i] - '0');
        }
        if (sz == 0) sz = 1024;
    }
    test_alloc_ptr = kmalloc(sz);
    if (test_alloc_ptr) {
        print_str("\nAllocated "); print_dec(sz);
        print_str(" bytes @ "); print_hex((uint32_t)test_alloc_ptr);
    } else {
        print_str("\nAllocation failed!");
    }
}

static void cmd_hexdump(const char* addr_str)
{
    uint32_t addr = 0;
    const char* p = addr_str;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
    }
    while (*p) {
        uint8_t nib;
        if (*p >= '0' && *p <= '9') {
            nib = *p - '0';
        } else if (*p >= 'a' && *p <= 'f') {
            nib = *p - 'a' + 10;
        } else if (*p >= 'A' && *p <= 'F') {
            nib = *p - 'A' + 10;
        } else {
            break;
        }
        addr = (addr << 4) | nib; 
        p++;
    }
    print_str("\nHexdump @ "); print_hex(addr); print_str(" :\n");
    uint8_t* mem = (uint8_t*)addr;
    for (int row = 0; row < 4; row++) {
        print_hex(addr + (uint32_t)(row * 16)); print_str("  ");
        for (int col = 0; col < 16; col++) {
            uint8_t b = mem[row * 16 + col];
            const char* h = "0123456789ABCDEF";
            terminal_putchar(h[b >> 4]);
            terminal_putchar(h[b & 0xF]);
            terminal_putchar(' ');
        }
        print_str(" |");
        for (int col = 0; col < 16; col++) {
            uint8_t b = mem[row * 16 + col];
            terminal_putchar((b >= 32 && b < 127) ? (char)b : '.');
        }
        print_str("|\n");
    }
}

static void cmd_calc(int argc, char args[MAX_ARGS][ARG_LEN])
{
    if (argc < 4) {
        print_str("\nUsage: calc <n> <op> <m>"); 
        return;
    }
    
    int32_t a = 0, b = 0; 
    int na = 0, nb = 0;
    const char* pa = args[1]; 
    const char* pb = args[3];
    
    if (*pa == '-') { na = 1; pa++; }
    if (*pb == '-') { nb = 1; pb++; }
    
    while (*pa >= '0' && *pa <= '9') {
        a = a * 10 + (*pa++ - '0');
    }
    while (*pb >= '0' && *pb <= '9') {
        b = b * 10 + (*pb++ - '0');
    }
    
    if (na) a = -a; 
    if (nb) b = -b;
    
    char op = args[2][0]; 
    int32_t res = 0; 
    int ok = 1;
    
    switch (op) {
        case '+': res = a + b; break;
        case '-': res = a - b; break;
        case '*': res = a * b; break;
        case '/':
            if (b == 0) { print_str("\nDiv/0!"); ok = 0; } 
            else { res = a / b; }
            break;
        default: print_str("\nInvalid op"); ok = 0;
    }
    
    if (ok) {
        print_str("\nResult: ");
        if (res < 0) { terminal_putchar('-'); res = -res; }
        print_dec((uint32_t)res);
    }
}

static void cmd_sem(void)
{
    semaphore_t s;
    sem_init(&s, 3, 3);
    print_str("\nSemaphore Demo (max=3):\n");
    print_str("  Init     : "); print_dec((uint32_t)sem_value(&s)); print_str("\n");
    sem_wait(&s);
    print_str("  wait()   : "); print_dec((uint32_t)sem_value(&s)); print_str("\n");
    sem_wait(&s);
    print_str("  wait()   : "); print_dec((uint32_t)sem_value(&s)); print_str("\n");
    sem_post(&s);
    print_str("  post()   : "); print_dec((uint32_t)sem_value(&s)); print_str("\n");
    int r = sem_trywait(&s);
    print_str("  trywait(): "); 
    print_str(r == 0 ? "ok" : "failed");
    print_str(" -> "); 
    print_dec((uint32_t)sem_value(&s)); 
    print_str("\n");
    sem_post(&s); 
    sem_post(&s);
    print_str("  2xpost() : "); print_dec((uint32_t)sem_value(&s)); print_str("\n");
}

static void cmd_sys(void)
{
    print_str("\n=== Syscall Test (int 0x80) ===\n");
    print_str("[SYS_WRITE  ] ");
    sys_print("Hello from syscall!\n");
    print_str("[SYS_TICKS  ] ticks = ");
    print_dec(sys_ticks()); print_str("\n");
    print_str("[SYS_GETPID ] pid = ");
    print_dec(sys_getpid()); print_str("\n");
}

static void spawned_task_fn(void)
{
    while (1) task_sleep(200);
}

static void cmd_spawn(int argc, char args[MAX_ARGS][ARG_LEN])
{
    const char* name = (argc >= 2) ? args[1] : "proc";
    int pid = process_create(name, spawned_task_fn, 1, 0);
    if (pid >= 0) {
        print_str("\nProcess '"); 
        print_str(name);
        print_str("' created, PID="); 
        print_dec((uint32_t)pid);
    } else {
        print_str("\nFailed: pool full.");
    }
}

static void cmd_reboot(void)
{
    print_str("\nReboot...");
    uint32_t zero[2] = {0, 0};
    __asm__ __volatile__(
        "cli\n"
        "lidt (%0)\n"
        "int $3\n"
        : : "r"(zero)
    );
}

/* ── shutdown : arrêt du système (NOUVEAU) ─────────────────────────
   Tente plusieurs méthodes d'extinction connues sous QEMU/Bochs.
   Si aucune ne fonctionne (ex: matériel réel sans ACPI configuré),
   on se rabat sur un arrêt logiciel propre : interruptions coupées
   et hlt en boucle (le CPU ne consomme plus de cycles utiles). */
static void cmd_shutdown(void)
{
    print_str("\nShutting down...\n");
    log_msg(LOG_INFO, "System shutdown requested");

    __asm__ __volatile__("cli");

    /* QEMU standard ACPI shutdown (port 0x604, valeur 0x2000) */
    sh_outw(0x604, 0x2000);

    /* QEMU/Bochs ancien style (port 0xB004, valeur 0x2000) */
    sh_outw(0xB004, 0x2000);

    /* VirtualBox */
    sh_outw(0x4004, 0x3400);

    /* Si on arrive ici, aucune méthode d'extinction n'a fonctionné */
    print_str("Power-off not supported on this platform.\n");
    print_str("System halted. You can now turn off the machine.\n");
    while (1) {
        __asm__ __volatile__("hlt");
    }
}

static void execute_command(void)
{
    buffer[buf_idx] = '\0';
    
    history_index = -1;
    saved_buf_idx = 0;
    saved_buffer[0] = '\0';
    
    if (buf_idx == 0) { print_prompt(); return; }

    history_add(buffer);

    char args[MAX_ARGS][ARG_LEN];
    int argc = 0;
    split_args(buffer, args, &argc);
    
    if (argc == 0) { print_prompt(); return; }

    if (sh_strcmp(args[0], "help") == 0) {
        cmd_help();
    } else if (sh_strcmp(args[0], "history") == 0) {
        cmd_history();
    } else if (sh_strcmp(args[0], "clear") == 0) {
        terminal_clear();
        draw_header();
    } else if (sh_strcmp(args[0], "uptime") == 0) {
        cmd_uptime();
    } else if (sh_strcmp(args[0], "ticks") == 0) {
        print_str("\nTicks: "); print_dec(get_ticks());
    } else if (sh_strcmp(args[0], "sysinfo") == 0) {
        cmd_sysinfo();
    } else if (sh_strcmp(args[0], "logs") == 0) {
        cmd_logs();
    } else if (sh_strcmp(args[0], "loglevel") == 0) {
        cmd_loglevel(argc, args);
    } else if (sh_strcmp(args[0], "logclear") == 0) {
        log_clear();
        print_str("\nLogs cleared.");
    } else if (sh_strcmp(args[0], "pages") == 0) {
        cmd_pages();
    } else if (sh_strcmp(args[0], "vmmap") == 0) {
        cmd_vmmap();
    } else if (sh_strcmp(args[0], "ring") == 0) {
        cmd_ring();
    } else if (sh_strcmp(args[0], "siglist") == 0) {
        cmd_siglist();
    } else if (sh_strcmp(args[0], "testsig") == 0) {
        cmd_testsig(argc, args);
    } else if (sh_strcmp(args[0], "tasks") == 0) {
        cmd_tasks();
    } else if (sh_strcmp(args[0], "ps") == 0) {
        cmd_ps();
    } else if (sh_strcmp(args[0], "spawn") == 0) {
        cmd_spawn(argc, args);
    } else if (sh_strcmp(args[0], "mem") == 0) {
        cmd_mem();
    } else if (sh_strcmp(args[0], "alloc") == 0) {
        cmd_alloc(argc, args);
    } else if (sh_strcmp(args[0], "free") == 0) {
        if (!test_alloc_ptr) { print_str("\nNothing to free."); } 
        else { kfree(test_alloc_ptr); test_alloc_ptr = 0; print_str("\nFreed."); }
    } else if (sh_strcmp(args[0], "hexdump") == 0) {
        if (argc < 2) {
            print_str("\nUsage: hexdump <addr>");
        } else {
            cmd_hexdump(args[1]);
        }
    } else if (sh_strcmp(args[0], "ls") == 0) {
        print_str("\nVFS Files:\n"); 
        vfs_list(print_str);
    } else if (sh_strcmp(args[0], "cat") == 0) {
        if (argc < 2) {
            print_str("\nUsage: cat <file>");
        } else {
            vfs_file_t* f = vfs_open(args[1]);
            if (f) { print_str("\n"); print_str(f->content); } 
            else { print_str("\nNot found: "); print_str(args[1]); }
        }
    } else if (sh_strcmp(args[0], "echo") == 0) {
        print_str("\n");
        for (int a = 1; a < argc; a++) {
            print_str(args[a]);
            if (a < argc - 1) terminal_putchar(' ');
        }
    } else if (sh_strcmp(args[0], "calc") == 0) {
        cmd_calc(argc, args);
    } else if (sh_strcmp(args[0], "sem") == 0) {
        cmd_sem();
    } else if (sh_strcmp(args[0], "sys") == 0) {
        cmd_sys();
    } else if (sh_strcmp(args[0], "reboot") == 0) {
        cmd_reboot();
    } else if (sh_strcmp(args[0], "shutdown") == 0) {
        cmd_shutdown();
    } else {
        print_str("\nUnknown: '"); print_str(args[0]);
        print_str("'  (type 'help')");
    }

    buf_idx = 0;
    print_prompt();
}

void init_shell(void)
{
    buf_idx = 0;
    history_count = 0;
    history_index = -1;
    saved_buf_idx = 0;
    saved_buffer[0] = '\0';
    
    for (int i = 0; i < HISTORY_SIZE; i++) {
        command_history[i][0] = '\0';
    }
    
    print_str("\n--- RTOS Shell v3.1 ---");
    print_str("\nType 'help' for help");
    print_prompt();
}

void shell_handle_key(char c)
{
    if (terminal_get_view_offset() != 0) {
        terminal_scroll_to_bottom();
    }
    
    if (c == '\n') {
        execute_command();
    } else if (c == '\b') {
        if (buf_idx > 0) { buf_idx--; buffer[buf_idx] = '\0'; terminal_putchar('\b'); }
    } else {
        if (buf_idx < MAX_BUFFER - 1) {
            buffer[buf_idx++] = c;
            buffer[buf_idx] = '\0';
            terminal_putchar(c);
        }
    }
}
