/*
================================================================================
FICHIER: shell.c
VERSION: 3.0 - Avec commandes avancées (logs, paging, signaux, rings)
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
   Fonctions utilitaires de chaînes
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
   Nouvelles commandes RTOS v3.0
   ────────────────────────────────────────────────────────────────── */

static void cmd_help(void)
{
    print_str("\n=== RTOS Shell v3.0 ===\n");
    print_str("\n  SYSTEME:\n");
    print_str("   help              - Cette aide\n");
    print_str("   clear             - Efface l'ecran\n");
    print_str("   uptime            - Temps depuis le boot\n");
    print_str("   ticks             - Ticks PIT courants\n");
    print_str("   reboot            - Redemarrage\n");
    print_str("   sysinfo           - Informations système\n");
    
    print_str("\n  LOGGING (NOUVEAU v3.0):\n");
    print_str("   logs              - Affiche le buffer de logs\n");
    print_str("   loglevel <0-4>    - Définit le niveau de log\n");
    print_str("   logclear          - Vide le buffer de logs\n");
    
    print_str("\n  PAGING (NOUVEAU v3.0):\n");
    print_str("   pages             - Stats pagination\n");
    print_str("   vmmap             - Affiche carte mémoire\n");
    
    print_str("\n  SIGNAUX & RINGS (NOUVEAU v3.0):\n");
    print_str("   ring              - Affiche Ring actuel\n");
    print_str("   siglist           - Liste les signaux\n");
    print_str("   testsig <pid>     - Envoie SIGTERM à un PID\n");
    
    print_str("\n  NAVIGATION:\n");
    print_str("   Page Up/Down      - Defiler vers haut/bas\n");
    print_str("   Fleche Haut/Bas   - Historique commandes\n");
    
    print_str("\n  TACHES & PROCESSUS:\n");
    print_str("   tasks             - Liste taches (scheduler)\n");
    print_str("   ps                - Liste processus (PCB)\n");
    print_str("   spawn <nom>       - Cree un processus\n");
    
    print_str("\n  MEMOIRE:\n");
    print_str("   mem               - Usage du tas\n");
    print_str("   alloc [octets]    - Alloue de la memoire\n");
    print_str("   free              - Libere la derniere alloc\n");
    print_str("   hexdump <addr>    - Dump hex 64 octets\n");
    
    print_str("\n  FICHIERS VFS:\n");
    print_str("   ls                - Liste les fichiers\n");
    print_str("   cat <fichier>     - Affiche le contenu\n");
    print_str("   touch <fichier>   - Cree un fichier vide\n");
    print_str("   write <f> <txt>   - Ecrit dans un fichier\n");
    print_str("   rm <fichier>      - Supprime un fichier\n");
    
    print_str("\n  DIVERS:\n");
    print_str("   echo <texte>      - Affiche du texte\n");
    print_str("   calc <n> <op> <m> - Calculatrice (+ - * /)\n");
    print_str("   sem               - Demo semaphore\n");
    print_str("   sys               - Test syscalls\n");
    print_str("   history           - Affiche l'historique\n");
}

static void cmd_history(void)
{
    print_str("\n=== Historique des commandes ===\n");
    if (history_count == 0) {
        print_str("  (vide)\n");
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
    print_str("\nUptime : ");
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
    print_str("Pages libres: ");
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
        print_str("\nNiveau de log changé à ");
        print_dec(level);
    }
}

static void cmd_pages(void)
{
    print_str("\n=== Statistiques Paging ===\n");
    print_str("Pages totales: ");
    print_dec(1024 * 1024);
    print_str("\nPages utilisées: ");
    print_dec(paging_get_used_pages());
    print_str("\nPages libres: ");
    print_dec(paging_get_free_pages());
    print_str("\n");
}

static void cmd_ring(void)
{
    uint32_t ring = get_current_ring();
    print_str("\nRing actuel: Ring ");
    print_dec(ring);
    if (ring == 0) {
        print_str(" (Kernel mode)\n");
    } else {
        print_str(" (User mode)\n");
    }
}

static void cmd_siglist(void)
{
    print_str("\n=== Signaux RTOS ===\n");
    print_str("SIGHUP  (1)   - Hangup\n");
    print_str("SIGINT  (2)   - Interruption (Ctrl+C)\n");
    print_str("SIGTERM (15)  - Terminaison\n");
    print_str("SIGKILL (9)   - Forcé (non bloquable)\n");
    print_str("SIGUSR1 (10)  - Signal utilisateur 1\n");
    print_str("SIGUSR2 (11)  - Signal utilisateur 2\n");
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
    print_str("\nSignal SIGTERM envoyé au PID ");
    print_dec(pid);
    print_str(" : ");
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
    print_str("\nHeap total : ");
    print_dec((uint32_t)(total / 1024)); print_str(" Ko\n");
    print_str("  Utilise  : "); print_dec((uint32_t)used);  print_str(" octets\n");
    print_str("  Libre    : "); print_dec((uint32_t)avail); print_str(" octets\n");
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
        print_str("\nDeja alloue ! Faites 'free' d'abord."); 
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
        print_str("\nAlloue "); print_dec(sz);
        print_str(" octets @ "); print_hex((uint32_t)test_alloc_ptr);
    } else {
        print_str("\nEchec allocation !");
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
        default: print_str("\nOp invalide"); ok = 0;
    }
    
    if (ok) {
        print_str("\nResultat : ");
        if (res < 0) { terminal_putchar('-'); res = -res; }
        print_dec((uint32_t)res);
    }
}

static void cmd_sem(void)
{
    semaphore_t s;
    sem_init(&s, 3, 3);
    print_str("\nDemo Semaphore (max=3) :\n");
    print_str("  Init     : "); print_dec((uint32_t)sem_value(&s)); print_str("\n");
    sem_wait(&s);
    print_str("  wait()   : "); print_dec((uint32_t)sem_value(&s)); print_str("\n");
    sem_wait(&s);
    print_str("  wait()   : "); print_dec((uint32_t)sem_value(&s)); print_str("\n");
    sem_post(&s);
    print_str("  post()   : "); print_dec((uint32_t)sem_value(&s)); print_str("\n");
    int r = sem_trywait(&s);
    print_str("  trywait(): "); 
    print_str(r == 0 ? "ok" : "echec");
    print_str(" -> "); 
    print_dec((uint32_t)sem_value(&s)); 
    print_str("\n");
    sem_post(&s); 
    sem_post(&s);
    print_str("  2xpost() : "); print_dec((uint32_t)sem_value(&s)); print_str("\n");
}

static void cmd_sys(void)
{
    print_str("\n=== Test Syscalls (int 0x80) ===\n");
    print_str("[SYS_WRITE  ] ");
    sys_print("Hello depuis syscall!\n");
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
        print_str("\nProcessus '"); 
        print_str(name);
        print_str("' cree, PID="); 
        print_dec((uint32_t)pid);
    } else {
        print_str("\nEchec : pool plein.");
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
        print_str("\nTicks : "); print_dec(get_ticks());
    } else if (sh_strcmp(args[0], "sysinfo") == 0) {
        cmd_sysinfo();
    } else if (sh_strcmp(args[0], "logs") == 0) {
        cmd_logs();
    } else if (sh_strcmp(args[0], "loglevel") == 0) {
        cmd_loglevel(argc, args);
    } else if (sh_strcmp(args[0], "logclear") == 0) {
        log_clear();
        print_str("\nLogs vidés.");
    } else if (sh_strcmp(args[0], "pages") == 0) {
        cmd_pages();
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
        if (!test_alloc_ptr) { print_str("\nRien a liberer."); } 
        else { kfree(test_alloc_ptr); test_alloc_ptr = 0; print_str("\nLibere."); }
    } else if (sh_strcmp(args[0], "hexdump") == 0) {
        if (argc < 2) {
            print_str("\nUsage: hexdump <addr>");
        } else {
            cmd_hexdump(args[1]);
        }
    } else if (sh_strcmp(args[0], "ls") == 0) {
        print_str("\nFichiers VFS:\n"); 
        vfs_list(print_str);
    } else if (sh_strcmp(args[0], "cat") == 0) {
        if (argc < 2) {
            print_str("\nUsage: cat <fichier>");
        } else {
            vfs_file_t* f = vfs_open(args[1]);
            if (f) { print_str("\n"); print_str(f->content); } 
            else { print_str("\nIntrouvable: "); print_str(args[1]); }
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
    } else {
        print_str("\nInconnu: '"); print_str(args[0]);
        print_str("'  (tapez 'help')");
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
    
    print_str("\n--- RTOS Shell v3.0 ---");
    print_str("\nTapez 'help' pour l'aide");
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
