#include "keyboard.h"
#include "idt.h"
#include "shell.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__ ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static const char scancode_qwerty[] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',0,
    '\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
};

static int extended = 0;

static void keyboard_callback(registers_t* regs) {
    (void)regs;
    uint8_t scancode = inb(0x60);

    if (scancode == 0xE0) { extended = 1; return; }

    if (extended) {
        extended = 0;
        if (!(scancode & 0x80)) {
            if (scancode == 0x48) { shell_handle_key(0x01); return; } /* fleche haut */
            if (scancode == 0x50) { shell_handle_key(0x02); return; } /* fleche bas */
        }
        return;
    }

    if (!(scancode & 0x80)) {
        if (scancode < sizeof(scancode_qwerty)) {
            char c = scancode_qwerty[scancode];
            if (c != 0) shell_handle_key(c);
        }
    }
}

void init_keyboard(void) {
    register_interrupt_handler(33, keyboard_callback);
}
