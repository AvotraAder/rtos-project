#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include "error.h"

/* ──────────────────────────────────────────────────────────────────
   Codes de syscalls RTOS
   ────────────────────────────────────────────────────────────────── */

#define SYS_WRITE       1       /* write(str) */
#define SYS_GETTICKS    2       /* getticks() -> uint32_t */
#define SYS_MALLOC      3       /* malloc(size) -> void* */
#define SYS_FREE        4       /* free(ptr) */
#define SYS_KILL        5       /* kill(pid, signum) */
#define SYS_PAUSE       6       /* pause() */
#define SYS_PIPE        7       /* pipe_create() */
#define SYS_PREAD       8       /* pipe_read(pipe, &data) */
#define SYS_PWRITE      9       /* pipe_write(pipe, data) */
#define SYS_GETPID      10      /* getpid() -> uint32_t */
#define SYS_GETPPID     11      /* getppid() -> uint32_t */
#define SYS_EXIT        12      /* exit(code) */
#define SYS_RING3       13      /* switch_to_ring3(entry, arg1, arg2) */
#define SYS_GETRING     14      /* get_ring() -> uint32_t */
#define SYS_SIGNAL      15      /* signal(signum, handler) */
#define SYS_SIGBLOCK    16      /* sigblock(mask) */

/* ──────────────────────────────────────────────────────────────────
   Wrappers pour syscalls
   ────────────────────────────────────────────────────────────────── */

void sys_print(const char* str);
uint32_t sys_ticks(void);
void* sys_kmalloc(size_t size);
void sys_kfree(void* ptr);
error_t sys_kill(uint32_t pid, int signum);
error_t sys_pause(void);
uint32_t sys_getpid(void);
uint32_t sys_getppid(void);
void sys_exit(uint32_t code);
uint32_t sys_getring(void);

#endif /* SYSCALL_H */
