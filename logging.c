#include "logging.h"
#include "serial.h"
#include <stddef.h>

#define LOG_BUFFER_SIZE   256
#define LOG_ENTRY_MAX     64
#define LOG_ENTRIES       32

typedef struct {
    char message[LOG_ENTRY_MAX];
    log_level_t level;
    uint32_t timestamp;
} log_entry_t;

static log_entry_t log_buffer[LOG_ENTRIES];
static int log_head = 0;
static int log_count = 0;
static log_level_t current_level = LOG_DEBUG;

extern uint32_t get_ticks(void);

static const char* level_to_string(log_level_t level)
{
    switch (level) {
        case LOG_DEBUG:     return "[DEBUG]";
        case LOG_INFO:      return "[INFO]";
        case LOG_WARN:      return "[WARN]";
        case LOG_ERROR:     return "[ERROR]";
        case LOG_CRITICAL:  return "[CRIT]";
        default:            return "[?]";
    }
}

static void safe_strcpy(char* dest, const char* src, size_t max)
{
    size_t i;
    for (i = 0; i < max - 1 && src[i]; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

void log_init(void)
{
    log_head = 0;
    log_count = 0;
    current_level = LOG_DEBUG;
    
    for (int i = 0; i < LOG_ENTRIES; i++) {
        log_buffer[i].message[0] = '\0';
        log_buffer[i].level = LOG_DEBUG;
        log_buffer[i].timestamp = 0;
    }
}

void log_set_level(log_level_t level)
{
    current_level = level;
}

log_level_t log_get_level(void)
{
    return current_level;
}

void log_msg(log_level_t level, const char* format, ...)
{
    if (level < current_level) {
        return;
    }
    
    char buffer[LOG_ENTRY_MAX];
    int pos = 0;
    
    uint32_t ticks = get_ticks();
    int tick_len = 0;
    uint32_t t = ticks;
    char tick_str[16];
    if (t == 0) {
        tick_str[0] = '0';
        tick_len = 1;
    } else {
        while (t > 0 && tick_len < 15) {
            tick_str[tick_len++] = '0' + (t % 10);
            t /= 10;
        }
        for (int i = 0; i < tick_len / 2; i++) {
            char tmp = tick_str[i];
            tick_str[i] = tick_str[tick_len - 1 - i];
            tick_str[tick_len - 1 - i] = tmp;
        }
    }
    tick_str[tick_len] = '\0';
    
    const char* level_str = level_to_string(level);
    
    pos = 0;
    while (level_str[pos] && pos < LOG_ENTRY_MAX - 1) {
        buffer[pos] = level_str[pos];
        pos++;
    }
    
    if (pos < LOG_ENTRY_MAX - 1) buffer[pos++] = ' ';
    if (pos < LOG_ENTRY_MAX - 1) buffer[pos++] = '@';
    
    int i = 0;
    while (tick_str[i] && pos < LOG_ENTRY_MAX - 1) {
        buffer[pos++] = tick_str[i++];
    }
    
    if (pos < LOG_ENTRY_MAX - 1) buffer[pos++] = ':';
    if (pos < LOG_ENTRY_MAX - 1) buffer[pos++] = ' ';
    
    i = 0;
    while (format[i] && pos < LOG_ENTRY_MAX - 1) {
        buffer[pos++] = format[i++];
    }
    buffer[pos] = '\0';
    
    log_buffer[log_head].level = level;
    log_buffer[log_head].timestamp = ticks;
    safe_strcpy(log_buffer[log_head].message, buffer, LOG_ENTRY_MAX);
    
    log_head = (log_head + 1) % LOG_ENTRIES;
    if (log_count < LOG_ENTRIES) {
        log_count++;
    }
    
    serial_write(buffer);
    serial_write("\n");
}

void log_dump(void (*print_fn)(const char*))
{
    print_fn("\n=== System Log Dump ===\n");
    
    if (log_count == 0) {
        print_fn("(vide)\n");
        return;
    }
    
    int start_idx = (log_count < LOG_ENTRIES) ? 0 : log_head;
    
    for (int i = 0; i < log_count; i++) {
        int idx = (start_idx + i) % LOG_ENTRIES;
        print_fn(log_buffer[idx].message);
        print_fn("\n");
    }
}

void log_clear(void)
{
    log_head = 0;
    log_count = 0;
    
    for (int i = 0; i < LOG_ENTRIES; i++) {
        log_buffer[i].message[0] = '\0';
        log_buffer[i].level = LOG_DEBUG;
        log_buffer[i].timestamp = 0;
    }
}
