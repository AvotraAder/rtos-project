#include "gdt.h"

/* Table de 6 entrées (Nul, CodeR0, DataR0, CodeR3, DataR3, TSS) */
gdt_entry_t gdt_entries[GDT_ENTRIES];
gdt_ptr_t   gdt_ptr;

/* Fonction assembleur externe pour charger la GDT */
extern void gdt_flush(uint32_t);

/* Fonction utilitaire pour configurer une entrée */
static void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;
    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access      = access;
}

void init_gdt(void) {
    gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    /* Entrée 0 : Segment Nul (obligatoire pour le processeur) */
    gdt_set_gate(0, 0, 0, 0, 0);

    /* Entrée 1 (sel 0x08) : Code Ring0, 0-4Go */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    /* Entrée 2 (sel 0x10) : Données Ring0, 0-4Go */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    /* Entrée 3 (sel 0x18) : Code Ring3, 0-4Go (DPL=3) */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    /* Entrée 4 (sel 0x20) : Données Ring3, 0-4Go (DPL=3) */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    /* Entrée 5 (sel 0x28) : TSS - placeholder, rempli par gdt_set_tss() */
    gdt_set_gate(5, 0, 0, 0, 0);

    /* On charge la GDT via l'assembleur */
    gdt_flush((uint32_t)&gdt_ptr);
}

void gdt_set_tss(uint32_t base, uint32_t limit) {
    /* Descripteur système 32-bit TSS disponible :
       présent=1, DPL=00, S=0 (système), type=1001 -> access=0x89
       granularité octet (limite tient sur 16 bits) -> gran=0x00 */
    gdt_set_gate(5, base, limit, 0x89, 0x00);
}
