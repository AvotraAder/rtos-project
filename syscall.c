#include "syscall.h"
#include "timer.h"
#include "heap.h"
#include "process.h"
#include "signal.h"
#include "ring.h"
#include "logging.h"
#include <stdint.h>

extern void terminal_putchar(char c);

static void print_str(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        terminal_putchar(str[i]);
    }
}

/* ──────────────────────────────────────────────────────────────────
   Handler principal des syscalls (appelé par int 0x80)
   ────────────────────────────────────────────────────────────────── */

uint32_t syscall_handler(uint32_t sys_num, uint32_t arg1, uint32_t arg2, uint32_t arg3) 
{
    (void)arg3;
    
    switch (sys_num) {
        case SYS_WRITE:
            print_str((const char*)arg1);
            return 0;
        
        case SYS_GETTICKS:
            return get_ticks();
        
        case SYS_MALLOC:
            return (uint32_t)kmalloc((size_t)arg1);
        
        case SYS_FREE:
            kfree((void*)arg1);
            return 0;
        
        case SYS_KILL: {
            error_t err = kill((uint32_t)arg1, (int)arg2);
            return (uint32_t)err;
        }
        
        case SYS_PAUSE: {
            process_sleep(1);
            return 0;
        }
        
        case SYS_GETPID: {
            process_t* current = process_current();
            return current ? current->pid : 0;
        }
        
        case SYS_GETPPID: {
            process_t* current = process_current();
            return current ? current->ppid : 0;
        }
        
        case SYS_EXIT: {
            process_t* current = process_current();
            if (current) {
                process_exit(current->pid);
            }
            return 0;
        }
        
        case SYS_GETRING: {
            return get_current_ring();
        }
        
        case SYS_SIGNAL: {
            /* arg1 = signum, arg2 = handler */
            signal_handler_t handler = (signal_handler_t)arg2;
            signal_handler_t old = signal((int)arg1, handler);
            return (uint32_t)old;
        }
        
        case SYS_SIGBLOCK: {
            error_t err = sigblock((uint32_t)arg1);
            return (uint32_t)err;
        }
        
        default:
            log_msg(LOG_WARN, "Syscall inconnu");
            return (uint32_t)-1;
    }
}

/* ──────────────────────────────────────────────────────────────────
   Wrappers de syscalls (appelés depuis le code utilisateur)
   ────────────────────────────────────────────────────────────────── */

void sys_print(const char* str) 
{
    __asm__ __volatile__ (
        "int $0x80"
        :
        : "a"(SYS_WRITE), "b"((uint32_t)str)
    );
}

uint32_t sys_ticks(void) 
{
    uint32_t ret;
    __asm__ __volatile__ (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETTICKS)
    );
    return ret;
}

void* sys_kmalloc(size_t size) 
{
    uint32_t ret;
    __asm__ __volatile__ (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_MALLOC), "b"((uint32_t)size)
    );
    return (void*)ret;
}

void sys_kfree(void* ptr) 
{
    __asm__ __volatile__ (
        "int $0x80"
        :
        : "a"(SYS_FREE), "b"((uint32_t)ptr)
    );
}

error_t sys_kill(uint32_t pid, int signum) 
{
    uint32_t ret;
    __asm__ __volatile__ (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_KILL), "b"((uint32_t)pid), "c"((uint32_t)signum)
    );
    return (error_t)ret;
}

error_t sys_pause(void) 
{
    uint32_t ret;
    __asm__ __volatile__ (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_PAUSE)
    );
    return (error_t)ret;
}

uint32_t sys_getpid(void) 
{
    uint32_t ret;
    __asm__ __volatile__ (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETPID)
    );
    return ret;
}

uint32_t sys_getppid(void) 
{
    uint32_t ret;
    __asm__ __volatile__ (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETPPID)
    );
    return ret;
}

void sys_exit(uint32_t code) 
{
    __asm__ __volatile__ (
        "int $0x80"
        :
        : "a"(SYS_EXIT), "b"(code)
    );
}
