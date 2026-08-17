#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>

#define SYS_WRITE 1
#define SYS_GETTICKS 2
#define SYS_MALLOC 3
#define SYS_FREE 4

void sys_print(const char* str);
uint32_t sys_ticks(void);
void* sys_kmalloc(size_t size);
void sys_kfree(void* ptr);

#endif
