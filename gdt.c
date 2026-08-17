#include "gdt.h"

/* Notre table de 3 entrées (Nul, Code, Données) et son pointeur */
gdt_entry_t gdt_entries[3];
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
    gdt_ptr.limit = (sizeof(gdt_entry_t) * 3) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    /* Entrée 0 : Segment Nul (obligatoire pour le processeur) */
    gdt_set_gate(0, 0, 0, 0, 0);                
    
    /* Entrée 1 : Segment de Code (Mémoire de 0 à 4Go) */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); 
    
    /* Entrée 2 : Segment de Données (Mémoire de 0 à 4Go) */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); 

    /* On charge la GDT via l'assembleur */
    gdt_flush((uint32_t)&gdt_ptr);
}
