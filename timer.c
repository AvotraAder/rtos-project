#include "timer.h"
#include "idt.h"

static volatile uint32_t timer_ticks    = 0;
static volatile uint32_t timer_freq_hz  = 100;

static timer_callback_t callbacks[TIMER_MAX_CALLBACKS];
static int              cb_count = 0;

/* ── Callback principal IRQ0 ─────────────────────────────────────── */
static void timer_irq_callback(registers_t* regs)
{
    (void)regs;
    timer_ticks++;

    /* Appel de tous les callbacks enregistrés */
    for (int i = 0; i < cb_count; i++) {
        if (callbacks[i])
            callbacks[i]();
    }
}

/* ── Initialisation ──────────────────────────────────────────────── */
void init_timer(uint32_t frequency)
{
    timer_freq_hz = frequency;
    cb_count      = 0;

    for (int i = 0; i < TIMER_MAX_CALLBACKS; i++)
        callbacks[i] = 0;

    register_interrupt_handler(32, timer_irq_callback);

    uint32_t divisor = 1193180 / frequency;

    /* Programmation du PIT canal 0, mode 3 (carré) */
    __asm__ __volatile__ (
        "outb %0, %1" : : "a"((uint8_t)0x36), "Nd"((uint16_t)0x43));
    __asm__ __volatile__ (
        "outb %0, %1" : : "a"((uint8_t)(divisor & 0xFF)), "Nd"((uint16_t)0x40));
    __asm__ __volatile__ (
        "outb %0, %1" : : "a"((uint8_t)((divisor >> 8) & 0xFF)), "Nd"((uint16_t)0x40));
}

/* ── Getters ─────────────────────────────────────────────────────── */
uint32_t get_ticks(void)
{
    return timer_ticks;
}

uint32_t get_uptime_sec(void)
{
    return timer_ticks / timer_freq_hz;
}

/* ── Enregistrement de callbacks ─────────────────────────────────── */
int timer_register_callback(timer_callback_t cb)
{
    if (cb_count >= TIMER_MAX_CALLBACKS) return -1;
    callbacks[cb_count++] = cb;
    return 0;
}

void timer_unregister_callback(timer_callback_t cb)
{
    for (int i = 0; i < cb_count; i++) {
        if (callbacks[i] == cb) {
            /* Décale */
            for (int j = i; j < cb_count - 1; j++)
                callbacks[j] = callbacks[j + 1];
            callbacks[--cb_count] = 0;
            return;
        }
    }
}

/* ── Attente active ──────────────────────────────────────────────── */
void timer_wait_ms(uint32_t ms)
{
    uint32_t ticks_needed = (ms * timer_freq_hz) / 1000;
    if (ticks_needed == 0) ticks_needed = 1;
    uint32_t start = timer_ticks;
    while ((timer_ticks - start) < ticks_needed)
        __asm__ __volatile__ ("hlt");
}
