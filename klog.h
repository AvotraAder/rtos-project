#ifndef KLOG_H
#define KLOG_H

void klog_init(void);
void klog(const char* msg);
void klog_dump(void (*print_fn)(const char* str));

#endif
