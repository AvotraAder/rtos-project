/*
================================================================================
FICHIER: keyboard.c
VERSION: 2.1 - Avec support Page Up/Down pour scroll
================================================================================
*/

#include "keyboard.h"
#include "idt.h"
#include "shell.h"

/* Fonctions externes du terminal */
extern void terminal_scroll_up(int lines);
extern void terminal_scroll_down(int lines);
extern void terminal_scroll_to_top(void);
extern void terminal_scroll_to_bottom(void);

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Scancodes speciaux */
#define SCANCODE_EXTENDED    0xE0
#define SCANCODE_PAGE_UP     0x49
#define SCANCODE_PAGE_DOWN   0x51
#define SCANCODE_HOME        0x47
#define SCANCODE_END         0x4F
#define SCANCODE_UP_ARROW    0x48
#define SCANCODE_DOWN_ARROW  0x50
#define SCANCODE_LEFT_CTRL   0x1D
#define SCANCODE_LEFT_CTRL_RELEASE 0x9D

/* Etat des modificateurs */
static int ctrl_pressed = 0;
static int extended_mode = 0;

static const char scancode_qwerty[] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',0,
    '\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
};

static void keyboard_callback(registers_t* regs)
{
    (void)regs;
    uint8_t scancode = inb(0x60);
    
    /* Detection du prefix extended (E0) */
    if (scancode == SCANCODE_EXTENDED) {
        extended_mode = 1;
        return;
    }
    
    /* Gestion Ctrl press */
    if (scancode == SCANCODE_LEFT_CTRL) {
        ctrl_pressed = 1;
        return;
    }
    
    /* Gestion Ctrl release */
    if (scancode == SCANCODE_LEFT_CTRL_RELEASE) {
        ctrl_pressed = 0;
        return;
    }
    
    /* Touche relachee (bit 7 = 1) ? */
    if (scancode & 0x80) {
        extended_mode = 0;
        return;
    }
    
    /* Traitement des touches extended */
    if (extended_mode) {
        extended_mode = 0;
        
        switch (scancode) {
            case SCANCODE_PAGE_UP:
                /* Page Up - Scroll vers le haut (10 lignes) */
                terminal_scroll_up(10);
                return;
                
            case SCANCODE_PAGE_DOWN:
                /* Page Down - Scroll vers le bas (10 lignes) */
                terminal_scroll_down(10);
                return;
                
            case SCANCODE_HOME:
                if (ctrl_pressed) {
                    /* Ctrl+Home - Aller au debut du buffer */
                    terminal_scroll_to_top();
                }
                return;
                
            case SCANCODE_END:
                if (ctrl_pressed) {
                    /* Ctrl+End - Aller a la fin du buffer */
                    terminal_scroll_to_bottom();
                }
                return;
                
            case SCANCODE_UP_ARROW:
                /* Fleche haut - Historique commande precedente */
                shell_history_up();
                return;
                
            case SCANCODE_DOWN_ARROW:
                /* Fleche bas - Historique commande suivante */
                shell_history_down();
                return;
        }
    }
    
    /* Traitement normal des touches */
    if (scancode < sizeof(scancode_qwerty)) {
        char c = scancode_qwerty[scancode];
        if (c != 0) {
            /* Auto-scroll vers le bas lors de la frappe */
            terminal_scroll_to_bottom();
            shell_handle_key(c);
        }
    }
}

void init_keyboard(void)
{
    ctrl_pressed = 0;
    extended_mode = 0;
    register_interrupt_handler(33, keyboard_callback);
}
