#ifndef PANIC_H
#define PANIC_H

#include <stdint.h>
#include "idt.h"

void panic(const char* message, registers_t* regs);
void panic_msg(const char* message, uint32_t code);
void init_panic_handlers(void);

#endif
