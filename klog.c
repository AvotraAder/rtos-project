#include "klog.h"
#include "serial.h"
#include <stddef.h>

#define KLOG_LINES 16
#define KLOG_LINE_LEN 64

static char log_buf[KLOG_LINES][KLOG_LINE_LEN];
static int log_count = 0;
static int log_head = 0;

static size_t k_strlen(const char* s) { size_t n = 0; while (s[n]) n++; return n; }

void klog_init(void) {
    log_count = 0;
    log_head = 0;
}

void klog(const char* msg) {
    serial_write(msg);
    serial_write("\n");

    char* slot = log_buf[log_head];
    size_t len = k_strlen(msg);
    if (len >= KLOG_LINE_LEN) len = KLOG_LINE_LEN - 1;
    for (size_t i = 0; i < len; i++) slot[i] = msg[i];
    slot[len] = '\0';

    log_head = (log_head + 1) % KLOG_LINES;
    if (log_count < KLOG_LINES) log_count++;
}

void klog_dump(void (*print_fn)(const char* str)) {
    int start = (log_count < KLOG_LINES) ? 0 : log_head;
    for (int i = 0; i < log_count; i++) {
        int idx = (start + i) % KLOG_LINES;
        print_fn(log_buf[idx]);
        print_fn("\n");
    }
}
