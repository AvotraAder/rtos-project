#include "ring.h"
#include "logging.h"
#include "gdt.h"

/* ──────────────────────────────────────────────────────────────────
   Variables globales
   ────────────────────────────────────────────────────────────────── */

static tss_t kernel_tss;
static uint32_t tss_selector = 0;

/* ──────────────────────────────────────────────────────────────────
   Fonctions internes
   ────────────────────────────────────────────────────────────────── */

static void setup_tss(tss_t* tss)
{
    uint8_t* ptr = (uint8_t*)tss;
    for (unsigned int i = 0; i < sizeof(tss_t); i++) {
        ptr[i] = 0;
    }
    
    /* Stack Ring 0 (noyau) - utilisé lorsque le CPU passe de Ring 3 à Ring 0 */
    tss->esp0 = 0x00400000;  /* Base noyau */
    tss->ss0 = 0x10;         /* Data segment Ring 0 */
    
    /* Note: Pas de champs esp3/ss3 dans la structure TSS matérielle x86. 
     * Le processeur récupère automatiquement le stack utilisateur depuis 
     * la pile d'interruption lors d'un 'iret'. */
    
    /* CR3 (Page directory) */
    tss->cr3 = 0;
    
    /* Flags */
    tss->eflags = 0x00000200;  /* IF = 1 (interrupts enabled) */
    
    /* Segments */
    tss->cs = 0x0B;  /* Code segment Ring 3 + RPL=3 */
    tss->ss = 0x23;  /* Data segment Ring 3 + RPL=3 */
    tss->ds = 0x23;
    tss->es = 0x23;
    tss->fs = 0x23;
    tss->gs = 0x23;
    
    /* I/O Map base */
    tss->iomap_base = sizeof(tss_t);
}

/* ──────────────────────────────────────────────────────────────────
   API publique
   ────────────────────────────────────────────────────────────────── */

error_t ring_init(void)
{
    log_msg(LOG_INFO, "Initialisation des anneaux x86");
    
    setup_tss(&kernel_tss);
    
    /* Charger le TSS via LTR (Load Task Register) */
    tss_selector = 0x28;  /* TSS selector (assume GDT entry 5) */
    
    __asm__ __volatile__ (
        "ltr %0"
        :
        : "r"((uint16_t)tss_selector)
    );
    
    log_msg(LOG_INFO, "Anneaux x86 initialisés");
    
    return E_OK;
}

error_t switch_to_ring3(void (*user_entry)(void), uint32_t arg1, uint32_t arg2)
{
    if (!user_entry) {
        return E_INVAL;
    }
    
    log_msg(LOG_DEBUG, "Basculement vers Ring 3 (User mode)");
    
    uint32_t esp3 = 0x10000000;  /* Stack utilisateur */
    
    /* Préparer la pile pour le retour vers Ring 3 */
    uint32_t* stack = (uint32_t*)esp3;
    *(--stack) = arg2;
    *(--stack) = arg1;
    *(--stack) = (uint32_t)user_entry;
    
    /* Utiliser iret pour basculer */
    __asm__ __volatile__ (
        "mov $0x23, %%ax\n"      /* Ring 3 data segment */
        "mov %%ax, %%ss\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        
        "mov %0, %%esp\n"         /* Stack utilisateur */
        
        "push $0x23\n"            /* SS Ring 3 */
        "push $0x10000000\n"      /* ESP Ring 3 */
        "push $0x202\n"           /* EFLAGS (IF=1) */
        "push $0x1B\n"            /* CS Ring 3 */
        "push %1\n"               /* EIP = user_entry */
        
        "iret\n"
        :
        : "r"((uint32_t)stack), "r"((uint32_t)user_entry)
        : "rax"
    );
    
    return E_OK;
}

void switch_to_ring0(void)
{
    /* Appelé automatiquement lors d'une interruption/syscall */
}

uint32_t get_current_ring(void)
{
    uint32_t cs;
    __asm__ __volatile__ ("mov %%cs, %0" : "=r"(cs));
    return cs & 0x3;  /* Les 2 bits de poids faible = RPL (Ring) */
}

int is_user_accessible(void* ptr, size_t len)
{
    uint32_t addr = (uint32_t)ptr;
    
    /* Utilisateur peut accéder 0x08000000 à 0xBFFFFFFF */
    uint32_t user_base = 0x08000000;
    uint32_t user_top = 0xC0000000;
    
    if (addr < user_base || (addr + len) > user_top) {
        return 0;
    }
    
    return 1;
}
