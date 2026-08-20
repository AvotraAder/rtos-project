#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/* ──────────────────────────────────────────────────────────────────
   6 entrées : Nul, Code Ring0, Data Ring0, Code Ring3, Data Ring3, TSS
   ────────────────────────────────────────────────────────────────── */
#define GDT_ENTRIES 6

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

/* Fonction publique pour initialiser la GDT (segments Ring0 + Ring3) */
void init_gdt(void);

/* Installe/rafraîchit le descripteur TSS (entrée 5, sélecteur 0x28).
   À appeler après init_gdt(), avant tout ltr. */
void gdt_set_tss(uint32_t base, uint32_t limit);

#endif
