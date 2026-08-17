#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/* Structure d'une entrée dans la table IDT (8 octets) */
struct idt_entry_struct {
 uint16_t base_low;    /* Adresse basse du handler (bits 0-15) */
 uint16_t sel;         /* Selecteur de segment de code dans la GDT (0x08) */
 uint8_t always0;      /* Doit toujours être à 0 */
 uint8_t flags;        /* Drapeaux de présence / privilège (0x8E pour interrupt gate) */
 uint16_t base_high;   /* Adresse haute du handler (bits 16-31) */
} __attribute__((packed));

typedef struct idt_entry_struct idt_entry_t;

/* Pointeur IDT (pour l'instruction 'lidt') */
struct idt_ptr_struct {
 uint16_t limit;
 uint32_t base;
} __attribute__((packed));

typedef struct idt_ptr_struct idt_ptr_t;

/* Structure décrivant l'état des registres au moment d'une interruption */
typedef struct {
 uint32_t ds;          /* Segment de données */
 uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; /* Registres poussés par pusha */
 uint32_t int_no, err_code; /* Numéro d'interruption et code d'erreur */
 uint32_t eip, cs, eflags, useresp, ss; /* Poussés automatiquement par le CPU */
} registers_t;

/* Prototype pour enregistrer un handler personnalisé */
typedef void (*isr_t)(registers_t*);

/* Fonctions publiques */
void register_interrupt_handler(uint8_t n, isr_t handler);
void init_idt(void);
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);

#endif
