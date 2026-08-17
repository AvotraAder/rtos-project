#ifndef SERIAL_H
#define SERIAL_H

void init_serial(void);
void serial_putc(char c);
void serial_write(const char* str);

#endif
