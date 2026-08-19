#ifndef RING_H
#define RING_H

#include <stdint.h>
#include <stddef.h>
#include "error.h"

/* ──────────────────────────────────────────────────────────────────
   Anneaux de privilège x86
   ────────────────────────────────────────────────────────────────── */

#define RING_KERNEL     0   /* Kernel mode - Accès complet */
#define RING_USER       3   /* User mode - Accès limité */

/* ──────────────────────────────────────────────────────────────────
   TSS (Task State Segment) - Nécessaire pour Ring switching
   ────────────────────────────────────────────────────────────────── */

typedef struct {
    uint32_t prev_tss;      /* TSS précédent (pour tâches imbriquées) */
    uint32_t esp0;          /* Stack pointer Ring 0 (noyau) */
    uint32_t ss0;           /* Segment Ring 0 */
    uint32_t esp1;          /* Stack pointer Ring 1 */
    uint32_t ss1;           /* Segment Ring 1 */
    uint32_t esp2;          /* Stack pointer Ring 2 */
    uint32_t ss2;           /* Segment Ring 2 */
    uint32_t cr3;           /* Page directory */
    uint32_t eip;           /* Instruction pointer */
    uint32_t eflags;        /* Flags */
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;           /* LDT selector */
    uint16_t trap;          /* Trap flag */
    uint16_t iomap_base;    /* I/O bitmap base */
    uint8_t  iomap[8192];   /* I/O permission bitmap */
} __attribute__((packed)) tss_t;

/* ──────────────────────────────────────────────────────────────────
   Context pour changement de ring
   ────────────────────────────────────────────────────────────────── */

typedef struct {
    uint32_t eip;           /* Entry point */
    uint32_t esp3;          /* User stack pointer */
    uint32_t arg1;          /* Premier argument */
    uint32_t arg2;          /* Deuxième argument */
} ring_context_t;

/* ──────────────────────────────────────────────────────────────────
   API Anneaux
   ────────────────────────────────────────────────────────────────── */

/* Initialisation */
error_t ring_init(void);

/* Basculer vers Ring 3 (User mode) */
error_t switch_to_ring3(void (*user_entry)(void), uint32_t arg1, uint32_t arg2);

/* Basculer vers Ring 0 (Kernel mode) - appelé automatiquement par les syscalls */
void switch_to_ring0(void);

/* Obtenir le ring actuel */
uint32_t get_current_ring(void);

/* Vérifier si une adresse est accessible en user mode */
int is_user_accessible(void* ptr, size_t len);

#endif /* RING_H */
