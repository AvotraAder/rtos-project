#include "shell.h"
#include "timer.h"
#include "task.h"
#include "heap.h"
#include "vfs.h"
#include "syscall.h"
#include <stdint.h>
#include <stddef.h>

extern void terminal_putchar(char c);
extern void terminal_write_at(const char* str, int row, int col);
extern void terminal_clear(void);

#define MAX_BUFFER 128

static char buffer[MAX_BUFFER];
static int buf_idx = 0;
static void* test_alloc_ptr = 0;

static int strcmp(const char* s1, const char* s2) {
 while (*s1 && (*s1 == *s2)) {
 s1++;
 s2++;
 }
 return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static int strncmp(const char* s1, const char* s2, size_t n) {
 while (n && *s1 && (*s1 == *s2)) {
 s1++;
 s2++;
 n--;
 }
 if (n == 0) return 0;
 return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static void print_str(const char* str) {
 for (size_t i = 0; str[i] != '\0'; i++) {
 terminal_putchar(str[i]);
 }
}

static void print_dec(uint32_t n) {
 if (n == 0) {
 terminal_putchar('0');
 return;
 }
 char buf[32];
 int i = 0;
 while (n > 0) {
 buf[i++] = '0' + (n % 10);
 n /= 10;
 }
 for (int j = i - 1; j >= 0; j--) {
 terminal_putchar(buf[j]);
 }
}

static void print_prompt(void) {
 print_str("\nRTOS> ");
}

static void execute_command(void) {
 buffer[buf_idx] = '\0';
 if (buf_idx == 0) {
 print_prompt();
 return;
 }
 
 if (strcmp(buffer, "help") == 0) {
 print_str("\nAvailable commands:\n");
 print_str(" help - Show command list\n");
 print_str(" clear - Clear terminal screen\n");
 print_str(" ticks - Show current timer ticks\n");
 print_str(" tasks - Show running tasks and priorities\n");
 print_str(" mem - Show heap memory usage\n");
 print_str(" alloc - Dynamically allocate 1024 bytes\n");
 print_str(" free - Free allocated memory\n");
 print_str(" ls - List RAMDisk files\n");
 print_str(" cat <file> - Read RAMDisk file\n");
 print_str(" touch <file> - Create empty file\n");
 print_str(" write <f> <t> - Write text to file\n");
 print_str(" rm <file> - Remove file\n");
 print_str(" sys - Test system calls (int 0x80)\n");
 print_str(" echo - Print text to screen");
 
 } else if (strcmp(buffer, "clear") == 0) {
 terminal_clear();
 terminal_write_at("--- RTOS Preemptive Multitasking + Interactive Shell ---", 0, 0);
 terminal_write_at("Task A (Producer Sent) : ", 2, 0);
 terminal_write_at("Task B (Consumer Recv) : ", 3, 0);
 
 } else if (strcmp(buffer, "ticks") == 0) {
 print_str("\nCurrent PIT Ticks: ");
 print_dec(get_ticks());
 
 } else if (strcmp(buffer, "tasks") == 0) {
 print_str("\nTasks Status:\n");
 task_t* start = get_current_task();
 task_t* curr = start;
 do {
 print_str(" [ID ");
 print_dec(curr->id);
 print_str("] Prio: ");
 print_dec(curr->priority);
 print_str(" | State: ");
 if (curr->state == TASK_READY) {
 print_str("READY\n");
 } else {
 print_str("SLEEPING (");
 print_dec(curr->sleep_ticks);
 print_str(" ticks left)\n");
 }
 curr = curr->next;
 } while (curr != start);
 
 } else if (strcmp(buffer, "mem") == 0) {
 print_str("\nKernel Heap Status:\n");
 print_str(" Used : ");
 print_dec(heap_get_used());
 print_str(" bytes\n");
 print_str(" Free : ");
 print_dec(heap_get_free());
 print_str(" bytes");
 
 } else if (strcmp(buffer, "alloc") == 0) {
 if (test_alloc_ptr != 0) {
 print_str("\nMemory already allocated! Use 'free' first.");
 } else {
 test_alloc_ptr = kmalloc(1024);
 if (test_alloc_ptr) {
 print_str("\nAllocated 1024 bytes successfully.");
 } else {
 print_str("\nAllocation failed!");
 }
 }
 
 } else if (strcmp(buffer, "free") == 0) {
 if (test_alloc_ptr == 0) {
 print_str("\nNo memory to free!");
 } else {
 kfree(test_alloc_ptr);
 test_alloc_ptr = 0;
 print_str("\nMemory freed successfully.");
 }
 
 } else if (strcmp(buffer, "ls") == 0) {
 print_str("\nRAMDisk Files:\n");
 vfs_list(print_str);
 
 } else if (strncmp(buffer, "cat ", 4) == 0) {
 const char* filename = buffer + 4;
 vfs_file_t* file = vfs_open(filename);
 if (file) {
 print_str("\n");
 print_str(file->content);
 } else {
 print_str("\nFile not found: ");
 print_str(filename);
 }
 
 } else if (strncmp(buffer, "touch ", 6) == 0) {
 const char* filename = buffer + 6;
 if (vfs_touch(filename) == 0) {
 print_str("\nFile created: ");
 print_str(filename);
 } else {
 print_str("\nFailed to create file.");
 }
 
 } else if (strncmp(buffer, "write ", 6) == 0) {
 char* args = buffer + 6;
 char* space = 0;
 for (int i = 0; args[i] != '\0'; i++) {
 if (args[i] == ' ') {
 space = &args[i];
 break;
 }
 }
 if (space) {
 *space = '\0';
 const char* filename = args;
 const char* content = space + 1;
 if (vfs_write(filename, content) == 0) {
 print_str("\nWrote to ");
 print_str(filename);
 } else {
 print_str("\nWrite failed.");
 }
 } else {
 print_str("\nUsage: write <file> <text>");
 }
 
 } else if (strncmp(buffer, "rm ", 3) == 0) {
 const char* filename = buffer + 3;
 if (vfs_remove(filename) == 0) {
 print_str("\nRemoved: ");
 print_str(filename);
 } else {
 print_str("\nFile not found.");
 }
 
 } else if (strcmp(buffer, "sys") == 0) {
 print_str("\n=== Testing System Calls (int 0x80) ===");
 print_str("\n[SYS_WRITE] Message via syscall: ");
 sys_print("Hello from syscall!");
 print_str("\n[SYS_GETTICKS] PIT Ticks via syscall: ");
 uint32_t ticks = sys_ticks();
 print_dec(ticks);
 print_str("\n[SYS_MALLOC] Allocating 512 bytes via syscall...");
 void* ptr = sys_kmalloc(512);
 if (ptr) {
 print_str("\nAllocation successful! Pointer: 0x");
 print_dec((uint32_t)ptr);
 sys_kfree(ptr);
 print_str("\nFreed via syscall.");
 } else {
 print_str("\nAllocation failed!");
 }
 
 } else if (strncmp(buffer, "echo ", 5) == 0) {
 print_str("\n");
 print_str(buffer + 5);
 
 } else {
 print_str("\nUnknown command. Type 'help' for available commands.");
 }
 
 buf_idx = 0;
 print_prompt();
}

void init_shell(void) {
 buf_idx = 0;
 print_str("\nType 'help' to get started.");
 print_prompt();
}

void shell_handle_key(char c) {
 if (c == '\n') {
 execute_command();
 } else if (c == '\b') {
 if (buf_idx > 0) {
 buf_idx--;
 terminal_putchar('\b');
 }
 } else {
 if (buf_idx < MAX_BUFFER - 1) {
 buffer[buf_idx++] = c;
 terminal_putchar(c);
 }
 }
}
