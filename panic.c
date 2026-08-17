#include "panic.h"
#include "serial.h"
#include <stddef.h>

extern void terminal_putchar(char c);

static const char* exception_names[32] = {
    "Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
    "Overflow", "Out of Bounds", "Invalid Opcode", "No Coprocessor",
    "Double Fault", "Coprocessor Segment Overrun", "Bad TSS", "Segment Not Present",
    "Stack Fault", "General Protection Fault", "Page Fault", "Unknown Interrupt",
    "Coprocessor Fault", "Alignment Check", "Machine Check", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved"
};

static void print_str(const char* s) {
    for (size_t i = 0; s[i]; i++) { terminal_putchar(s[i]); serial_putc(s[i]); }
}

static void print_hex(uint32_t n) {
    char buf[11] = "0x00000000";
    for (int i = 9; i >= 2; i--) {
        uint8_t nibble = n & 0xF;
        buf[i] = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
        n >>= 4;
    }
    print_str(buf);
}

void panic(const char* message, registers_t* regs) {
    __asm__ __volatile__("cli");
    print_str("\n\n*** KERNEL PANIC ***\n");
    print_str(message);
    if (regs) {
        print_str("\nEIP: "); print_hex(regs->eip);
        print_str("  INT: "); print_hex(regs->int_no);
        print_str("  ERR: "); print_hex(regs->err_code);
        print_str("\nEAX: "); print_hex(regs->eax);
        print_str("  EBX: "); print_hex(regs->ebx);
        print_str("  ECX: "); print_hex(regs->ecx);
        print_str("  EDX: "); print_hex(regs->edx);
    }
    print_str("\nSystem halted.\n");
    while (1) { __asm__ __volatile__("hlt"); }
}

void panic_msg(const char* message, uint32_t code) {
    __asm__ __volatile__("cli");
    print_str("\n\n*** KERNEL PANIC ***\n");
    print_str(message);
    print_str(" (code: ");
    print_hex(code);
    print_str(")\nSystem halted.\n");
    while (1) { __asm__ __volatile__("hlt"); }
}

static void exception_handler(registers_t* regs) {
    const char* name = (regs->int_no < 32) ? exception_names[regs->int_no] : "Unknown";
    panic(name, regs);
}

void init_panic_handlers(void) {
    for (uint8_t i = 0; i < 32; i++) {
        register_interrupt_handler(i, exception_handler);
    }
}
