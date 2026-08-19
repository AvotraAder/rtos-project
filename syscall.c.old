#include "syscall.h"
#include "timer.h"
#include "heap.h"
#include <stdint.h>

extern void terminal_putchar(char c);

static void print_str(const char* str) {
 for (size_t i = 0; str[i] != '\0'; i++) {
 terminal_putchar(str[i]);
 }
}

uint32_t syscall_handler(uint32_t sys_num, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
 (void)arg2;
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
 
 default:
 return (uint32_t)-1;
 }
}

void sys_print(const char* str) {
 __asm__ __volatile__ (
 "int $0x80"
 :
 : "a"(SYS_WRITE), "b"((uint32_t)str)
 );
}

uint32_t sys_ticks(void) {
 uint32_t ret;
 __asm__ __volatile__ (
 "int $0x80"
 : "=a"(ret)
 : "a"(SYS_GETTICKS)
 );
 return ret;
}

void* sys_kmalloc(size_t size) {
 uint32_t ret;
 __asm__ __volatile__ (
 "int $0x80"
 : "=a"(ret)
 : "a"(SYS_MALLOC), "b"((uint32_t)size)
 );
 return (void*)ret;
}

void sys_kfree(void* ptr) {
 __asm__ __volatile__ (
 "int $0x80"
 :
 : "a"(SYS_FREE), "b"((uint32_t)ptr)
 );
}
