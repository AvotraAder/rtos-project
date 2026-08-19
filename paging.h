#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include "error.h"

/* ──────────────────────────────────────────────────────────────────
   Constantes de pagination
   ────────────────────────────────────────────────────────────────── */

#define PAGE_SIZE           4096        /* Taille d'une page (4 Ko) */
#define PAGES_PER_TABLE     1024        /* 1024 entrées par table */
#define PAGE_TABLES_COUNT   1024        /* 1024 tables de pages */
#define TOTAL_PAGES         (1024 * 1024) /* 1M de pages (4 Go) */

/* Flags des entrées de page */
#define PAGE_PRESENT        0x001       /* Page en mémoire */
#define PAGE_WRITE          0x002       /* Page accessible en écriture */
#define PAGE_USER           0x004       /* Page accessible en Ring 3 (User) */
#define PAGE_PWT            0x008       /* Write-Through */
#define PAGE_PCD            0x010       /* Cache Disabled */
#define PAGE_ACCESSED       0x020       /* Page accédée */
#define PAGE_DIRTY          0x040       /* Page modifiée */
#define PAGE_PAT            0x080       /* Page Attribute Table */
#define PAGE_GLOBAL         0x100       /* Page globale (non flush TLB) */

/* Adresses spéciales */
#define KERNEL_BASE         0x00400000  /* Base noyau : 4 Mo */
#define KERNEL_HEAP_BASE    0x01000000  /* Tas noyau : 16 Mo */
#define USER_BASE           0x08000000  /* Code utilisateur : 128 Mo */
#define USER_HEAP_BASE      0x20000000  /* Tas utilisateur : 512 Mo */
#define USER_STACK_TOP      0xC0000000  /* Stack utilisateur : 3 Go */

/* ──────────────────────────────────────────────────────────────────
   Structures de données
   ────────────────────────────────────────────────────────────────── */

typedef struct {
    uint32_t* page_directory;
    uint32_t* page_tables[PAGE_TABLES_COUNT];
    uint32_t num_pages;
    uint32_t num_free_pages;
} paging_t;

typedef struct {
    uint32_t virtual_addr;
    uint32_t physical_addr;
    uint32_t flags;
} page_mapping_t;

/* ──────────────────────────────────────────────────────────────────
   API Paging
   ────────────────────────────────────────────────────────────────── */

/* Initialisation */
error_t paging_init(void);
void paging_enable(void);

/* Allocation/Libération de pages */
void* alloc_page(void);
void* alloc_pages(uint32_t count);
error_t free_page(void* ptr);
error_t free_pages(void* ptr, uint32_t count);

/* Mapping mémoire virtuelle */
error_t map_page(uint32_t virtual, uint32_t physical, uint32_t flags);
error_t unmap_page(uint32_t virtual);
uint32_t get_physical_addr(uint32_t virtual);

/* Statistiques */
uint32_t paging_get_free_pages(void);
uint32_t paging_get_used_pages(void);

/* Allocation intelligente avec flags */
void* alloc_page_user(void);       /* Page accessible en Ring 3 */
void* alloc_page_kernel(void);     /* Page Ring 0 uniquement */

#endif /* PAGING_H */
