#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/* Structure d'une entrée de la GDT (8 octets) */
struct gdt_entry_struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));
typedef struct gdt_entry_struct gdt_entry_t;

/* Pointeur de la GDT (passé à l'instruction assembleur lgdt) */
struct gdt_ptr_struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));
typedef struct gdt_ptr_struct gdt_ptr_t;

/* Fonction publique pour initialiser la GDT */
void init_gdt(void);

#endif
