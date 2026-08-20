/*
================================================================================
FICHIER: kernel.c (VERSION 3.1)
VERSION: 3.1 - Avec Logging, Paging, Signaux, Rings
================================================================================
*/

#include <stdint.h>
#include <stddef.h>
#include "gdt.h"
#include "idt.h"
#include "timer.h"
#include "task.h"
#include "process.h"
#include "keyboard.h"
#include "shell.h"
#include "mutex.h"
#include "queue.h"
#include "heap.h"
#include "vfs.h"
#include "syscall.h"
#include "semaphore.h"
#include "logging.h"
#include "error.h"
#include "paging.h"
#include "signal.h"
#include "ring.h"
#include "panic.h"

static uint16_t* const VGA_BUFFER = (uint16_t*)0xB8000;

#define VGA_WIDTH   80
#define VGA_HEIGHT  25

/* Black & White theme only (no green/blue):
   - Terminal body   : white text on black background
   - Header          : black background, white text (with underline-like border)
   - Taskbar         : black background, white text */
#define VGA_COLOR_DEFAULT 0x0F   /* Black bg / White fg */
#define VGA_COLOR_HEADER  0x0F   /* Black bg / White fg */
#define VGA_COLOR_TASKBAR 0x0F   /* Black bg / White fg */

#define SCROLLBACK_LINES  200
#define HEADER_LINES      5
#define VISIBLE_LINES     (VGA_HEIGHT - HEADER_LINES)

static char scrollback_buffer[SCROLLBACK_LINES][VGA_WIDTH + 1];
static uint8_t scrollback_colors[SCROLLBACK_LINES];
static int scrollback_head = 0;
static int scrollback_count = 0;
static int view_offset = 0;
static int term_col = 0;

static mutex_t vga_mutex;
static queue_t msg_queue;

/* ──────────────────────────────────────────────────────────────────
   VGA Terminal Functions
   ────────────────────────────────────────────────────────────────── */

static inline uint16_t vga_entry(char c, uint8_t color)
{
    return (uint16_t)c | ((uint16_t)color << 8);
}

static inline void vga_outb(uint16_t port, uint8_t val)
{
    __asm__ __volatile__ ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* The VGA controller (CRTC) shows by default a blinking hardware
   cursor, independent of the software '_' drawn by the shell.
   Since BIOS/GRUB leaves it at an arbitrary position (often in the
   middle of the screen), it appears as a second "ghost" cursor.
   We disable it here: CRTC register 0x0A, bit 5 = disable. */
static void vga_disable_hw_cursor(void)
{
    vga_outb(0x3D4, 0x0A);
    vga_outb(0x3D5, 0x20);
}

static void vga_put_at(int row, int col, char c, uint8_t color)
{
    if (row >= 0 && row < VGA_HEIGHT && col >= 0 && col < VGA_WIDTH) {
        VGA_BUFFER[row * VGA_WIDTH + col] = vga_entry(c, color);
    }
}

static void vga_clear_row(int row, uint8_t color)
{
    for (int x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[row * VGA_WIDTH + x] = vga_entry(' ', color);
    }
}

static void scrollback_init(void)
{
    for (int i = 0; i < SCROLLBACK_LINES; i++) {
        for (int j = 0; j <= VGA_WIDTH; j++) {
            scrollback_buffer[i][j] = '\0';
        }
        scrollback_colors[i] = VGA_COLOR_DEFAULT;
    }
    scrollback_head = 0;
    scrollback_count = 0;
    view_offset = 0;
    term_col = 0;
}

static void scrollback_add_line(void)
{
    scrollback_buffer[scrollback_head][term_col] = '\0';
    scrollback_head = (scrollback_head + 1) % SCROLLBACK_LINES;
    
    if (scrollback_count < SCROLLBACK_LINES) {
        scrollback_count++;
    }
    
    for (int i = 0; i <= VGA_WIDTH; i++) {
        scrollback_buffer[scrollback_head][i] = '\0';
    }
    scrollback_colors[scrollback_head] = VGA_COLOR_DEFAULT;
    
    term_col = 0;
    view_offset = 0;
}

static void scrollback_putchar(char c, uint8_t color)
{
    if (term_col < VGA_WIDTH) {
        scrollback_buffer[scrollback_head][term_col] = c;
        scrollback_colors[scrollback_head] = color;
        term_col++;
    }
}

static int scrollback_get_line_index(int display_line)
{
    if (scrollback_count == 0) return -1;
    
    int lines_from_end = (VISIBLE_LINES - 1 - display_line) + view_offset;
    
    if (lines_from_end >= scrollback_count) return -1;
    
    int idx = scrollback_head - lines_from_end;
    if (idx < 0) idx += SCROLLBACK_LINES;
    
    return idx;
}

static void terminal_refresh_view(void)
{
    for (int row = 0; row < VISIBLE_LINES; row++) {
        int screen_row = HEADER_LINES + row;
        int buf_idx = scrollback_get_line_index(row);
        
        vga_clear_row(screen_row, VGA_COLOR_DEFAULT);
        
        if (buf_idx >= 0) {
            char* line = scrollback_buffer[buf_idx];
            uint8_t color = scrollback_colors[buf_idx];
            
            for (int col = 0; col < VGA_WIDTH && line[col]; col++) {
                vga_put_at(screen_row, col, line[col], color);
            }
        }
    }
    
    if (view_offset == 0) {
        int cursor_row = HEADER_LINES + VISIBLE_LINES - 1;
        vga_put_at(cursor_row, term_col, '_', 0x0F);
    }
    
    if (view_offset > 0) {
        const char* indicator = "^MORE";
        for (int i = 0; i < 5; i++) {
            vga_put_at(HEADER_LINES, VGA_WIDTH - 6 + i, indicator[i], 0x0F);
        }
    }
    
    if (scrollback_count > VISIBLE_LINES && view_offset < scrollback_count - VISIBLE_LINES) {
        const char* indicator = "vMORE";
        for (int i = 0; i < 5; i++) {
            vga_put_at(VGA_HEIGHT - 1, VGA_WIDTH - 6 + i, indicator[i], 0x0F);
        }
    }
}

void terminal_clear(void)
{
    scrollback_init();
    
    for (int y = 0; y < VGA_HEIGHT; y++) {
        vga_clear_row(y, VGA_COLOR_DEFAULT);
    }
}

void terminal_putchar(char c)
{
    if (c == '\n') {
        scrollback_add_line();
        terminal_refresh_view();
    } else if (c == '\b') {
        if (term_col > 0) {
            term_col--;
            scrollback_buffer[scrollback_head][term_col] = '\0';
            terminal_refresh_view();
        }
    } else {
        scrollback_putchar(c, VGA_COLOR_DEFAULT);
        if (term_col >= VGA_WIDTH) {
            scrollback_add_line();
        }
        terminal_refresh_view();
    }
}

void terminal_write_at(const char* str, int row, int col)
{
    for (size_t i = 0; str[i]; i++) {
        vga_put_at(row, col + i, str[i], VGA_COLOR_DEFAULT);
    }
}

void terminal_scroll_up(int lines)
{
    int max_offset = scrollback_count - VISIBLE_LINES;
    if (max_offset < 0) max_offset = 0;
    
    view_offset += lines;
    if (view_offset > max_offset) {
        view_offset = max_offset;
    }
    
    terminal_refresh_view();
}

void terminal_scroll_down(int lines)
{
    view_offset -= lines;
    if (view_offset < 0) {
        view_offset = 0;
    }
    
    terminal_refresh_view();
}

void terminal_scroll_to_top(void)
{
    int max_offset = scrollback_count - VISIBLE_LINES;
    if (max_offset < 0) max_offset = 0;
    view_offset = max_offset;
    terminal_refresh_view();
}

void terminal_scroll_to_bottom(void)
{
    view_offset = 0;
    terminal_refresh_view();
}

int terminal_get_view_offset(void)
{
    return view_offset;
}

int terminal_get_total_lines(void)
{
    return scrollback_count;
}

static void terminal_write_at_col(const char* str, int row, int col, uint8_t color)
{
    for (size_t i = 0; str[i]; i++) {
        vga_put_at(row, col + i, str[i], color);
    }
}

static void terminal_write_dec(uint32_t n, int row, int col)
{
    char buf[12]; 
    int i = 0;
    if (n == 0) {
        vga_put_at(row, col, '0', VGA_COLOR_DEFAULT);
        vga_put_at(row, col + 1, ' ', VGA_COLOR_DEFAULT);
        return;
    }
    while (n > 0) { 
        buf[i++] = '0' + (n % 10); 
        n /= 10; 
    }
    for (int j = 0; j < i; j++) {
        vga_put_at(row, col + j, buf[i - 1 - j], VGA_COLOR_DEFAULT);
    }
    vga_put_at(row, col + i, ' ', VGA_COLOR_DEFAULT);
}

void draw_header(void)
{
    const char* title = "  RTOS x86 v3.1  ";
    
    for (int x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[x] = vga_entry(' ', VGA_COLOR_HEADER);
    }
    terminal_write_at_col(title, 0, 0, VGA_COLOR_HEADER);

    for (int x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[VGA_WIDTH + x] = vga_entry('-', VGA_COLOR_DEFAULT);
    }

    terminal_write_at_col("Task A (Producer Sent) : ", 2, 0, VGA_COLOR_TASKBAR);
    terminal_write_at_col("Task B (Consumer Recv) : ", 3, 0, VGA_COLOR_TASKBAR);

    for (int x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[4 * VGA_WIDTH + x] = vga_entry('-', VGA_COLOR_DEFAULT);
    }
}

static void task_a(void)
{
    uint32_t count = 0;
    while (1) {
        queue_push(&msg_queue, count);
        mutex_lock(&vga_mutex);
        terminal_write_dec(count, 2, 26);
        mutex_unlock(&vga_mutex);
        count++;
        task_sleep(50);
    }
}

static void task_b(void)
{
    uint32_t val = 0;
    while (1) {
        if (queue_pop(&msg_queue, &val)) {
            mutex_lock(&vga_mutex);
            terminal_write_dec(val, 3, 26);
            mutex_unlock(&vga_mutex);
        }
        task_sleep(75);
    }
}

static void uptime_display_cb(void)
{
    static uint32_t last_sec = 0;
    uint32_t sec = get_uptime_sec();
    if (sec == last_sec) return;
    last_sec = sec;

    char buf[16]; 
    int i = 0;
    uint32_t s = sec, m = s / 60, h = m / 60;
    if (h > 0) {
        if (h >= 10) buf[i++] = '0' + h / 10;
        buf[i++] = '0' + h % 10;
        buf[i++] = 'h';
    }
    if ((m % 60) >= 10) buf[i++] = '0' + (m % 60) / 10;
    buf[i++] = '0' + (m % 60) % 10;
    buf[i++] = 'm';
    if ((s % 60) >= 10) buf[i++] = '0' + (s % 60) / 10;
    buf[i++] = '0' + (s % 60) % 10;
    buf[i++] = 's';
    buf[i] = '\0';

    int col = VGA_WIDTH - i - 1;
    for (int j = 0; j < i; j++) {
        VGA_BUFFER[VGA_WIDTH + col + j] = vga_entry(buf[j], VGA_COLOR_DEFAULT);
    }
}

/* ──────────────────────────────────────────────────────────────────
   Kernel main entry point
   ────────────────────────────────────────────────────────────────── */

void kernel_main(void)
{
    /* Base initialization */
    init_gdt();
    init_idt();

    /* IMPORTANT: register panic handlers BEFORE any risky subsystem
       (paging, ring). Without this, a CPU exception (#GP, #PF, ...)
       is silently re-executed in an infinite loop by the default
       ISR, and the screen stays frozen with no message. With these
       handlers, a fault shows a usable panic screen instead of
       freezing without explanation. */
    init_panic_handlers();

    terminal_clear();
    vga_disable_hw_cursor();
    draw_header();
    scrollback_init();
    
    /* New subsystems: Logging, Paging, Signals, Rings */
    log_init();
    log_msg(LOG_INFO, "RTOS v3.1 starting");
    
    error_t err = paging_init();
    if (err != E_OK) {
        log_msg(LOG_ERROR, "Paging init failed");
    } else {
        log_msg(LOG_INFO, "Paging initialized");
    }
    
    err = signal_init();
    if (err != E_OK) {
        log_msg(LOG_ERROR, "Signal init failed");
    } else {
        log_msg(LOG_INFO, "Signals initialized");
    }
    
    err = ring_init();
    if (err != E_OK) {
        log_msg(LOG_ERROR, "Ring init failed");
    } else {
        log_msg(LOG_INFO, "x86 Rings initialized");
    }
    
    /* Remaining initialization */
    init_timer(100);
    timer_register_callback(uptime_display_cb);
    init_keyboard();
    init_heap();
    vfs_init();
    mutex_init(&vga_mutex);
    queue_init(&msg_queue);
    init_tasking();
    create_task(task_a, 3);
    create_task(task_b, 2);
    process_init();
    init_shell();
    
    log_msg(LOG_INFO, "Enabling interrupts");
    __asm__ __volatile__("sti");
    
    log_msg(LOG_INFO, "Kernel ready. CLI active.");
    
    while (1) __asm__ __volatile__("hlt");
}
