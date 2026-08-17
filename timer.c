#include "timer.h"
#include "idt.h"

static volatile uint32_t timer_ticks = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__ ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void timer_callback(registers_t* regs) {
    (void)regs;
    timer_ticks++;
}

void init_timer(uint32_t frequency) {
    register_interrupt_handler(32, timer_callback);

    uint32_t divisor = 1193180 / frequency;

    outb(0x43, 0x36);

    uint8_t l = (uint8_t)(divisor & 0xFF);
    uint8_t h = (uint8_t)((divisor >> 8) & 0xFF);

    outb(0x40, l);
    outb(0x40, h);
}

uint32_t get_ticks(void) {
    return timer_ticks;
}
