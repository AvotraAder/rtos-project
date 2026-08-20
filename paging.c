#include "paging.h"
#include "logging.h"
#include "mutex.h"
#include <stddef.h>   /* size_t */

/* ──────────────────────────────────────────────────────────────────
   Mapping identité statique
   ──────────────────────────────────────────────────────────────────
   IMPORTANT : paging_enable() charge CR3 avec kernel_paging.page_directory.
   Si ce répertoire n'est pas rempli avec des entrées PRESENT valides
   AVANT l'activation (CR0.PG=1), la toute première instruction/accès
   mémoire suivant déclenche un #PF. Comme aucun handler n'est encore
   robuste à ce stade, cela boucle indéfiniment (écran figé).

   On identity-map donc les premiers IDENTITY_MAP_MB Mo (adresse
   virtuelle == adresse physique) pour couvrir le noyau, ses piles,
   le tas et le buffer VGA, AVANT d'activer la pagination.
   ────────────────────────────────────────────────────────────────── */

#define IDENTITY_MAP_MB    64                      /* Doit couvrir >= -m qemu */
#define NUM_PAGE_TABLES    (IDENTITY_MAP_MB / 4)    /* 1 table = 4 Mo */

static uint32_t page_directory[1024] __attribute__((aligned(4096)));
static uint32_t identity_page_tables[NUM_PAGE_TABLES][1024] __attribute__((aligned(4096)));

static paging_t kernel_paging;
static mutex_t paging_lock;
static uint8_t page_bitmap[TOTAL_PAGES / 8];  /* Bitmap pour suivi */

/* ──────────────────────────────────────────────────────────────────
   Fonctions internes
   ────────────────────────────────────────────────────────────────── */

static void paging_lock_page(uint32_t page_num)
{
    uint32_t byte_idx = page_num / 8;
    uint8_t bit_idx = page_num % 8;
    if (byte_idx < sizeof(page_bitmap)) {
        page_bitmap[byte_idx] |= (1 << bit_idx);
    }
}

static void paging_unlock_page(uint32_t page_num)
{
    uint32_t byte_idx = page_num / 8;
    uint8_t bit_idx = page_num % 8;
    if (byte_idx < sizeof(page_bitmap)) {
        page_bitmap[byte_idx] &= ~(1 << bit_idx);
    }
}

static int paging_is_page_free(uint32_t page_num)
{
    uint32_t byte_idx = page_num / 8;
    uint8_t bit_idx = page_num % 8;
    if (byte_idx < sizeof(page_bitmap)) {
        return !(page_bitmap[byte_idx] & (1 << bit_idx));
    }
    return 0;
}

static void paging_flush_tlb(void)
{
    uint32_t cr3;
    __asm__ __volatile__ ("mov %%cr3, %0" : "=r"(cr3));
    __asm__ __volatile__ ("mov %0, %%cr3" : : "r"(cr3));
}

static void paging_set_cr0(void)
{
    uint32_t cr0;
    __asm__ __volatile__ ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;  /* Enable paging bit */
    __asm__ __volatile__ ("mov %0, %%cr0" : : "r"(cr0));
}

/* ──────────────────────────────────────────────────────────────────
   API Paging publique
   ────────────────────────────────────────────────────────────────── */

error_t paging_init(void)
{
    log_msg(LOG_INFO, "Initialisation de la pagination...");

    mutex_init(&paging_lock);

    for (size_t i = 0; i < sizeof(page_bitmap); i++) {
        page_bitmap[i] = 0;
    }

    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 0;
    }
    for (int i = 0; i < PAGE_TABLES_COUNT; i++) {
        kernel_paging.page_tables[i] = 0;
    }

    /* Identity map des IDENTITY_MAP_MB premiers Mo : virt == phys.
       Couvre le noyau, ses piles de tâches/processus, le tas
       (HEAP_START = 16 Mo) et le buffer VGA (0xB8000). */
    for (int t = 0; t < NUM_PAGE_TABLES; t++) {
        for (int e = 0; e < 1024; e++) {
            uint32_t phys = (uint32_t)(t) * 4096u * 1024u + (uint32_t)(e) * 4096u;
            identity_page_tables[t][e] = (phys & 0xFFFFF000) | PAGE_PRESENT | PAGE_WRITE;
        }
        page_directory[t] = ((uint32_t)&identity_page_tables[t][0]) | PAGE_PRESENT | PAGE_WRITE;
        kernel_paging.page_tables[t] = &identity_page_tables[t][0];
    }

    kernel_paging.page_directory  = page_directory;
    kernel_paging.num_pages       = TOTAL_PAGES;
    kernel_paging.num_free_pages  = TOTAL_PAGES;

    /* Réserver (marquer utilisées) les pages physiques déjà
       identity-mappées pour que l'allocateur bitmap ne les
       redistribue jamais par-dessus le noyau/tas. */
    uint32_t reserved_pages = (IDENTITY_MAP_MB * 1024u * 1024u) / PAGE_SIZE;
    if (reserved_pages > TOTAL_PAGES) reserved_pages = TOTAL_PAGES;
    for (uint32_t p = 0; p < reserved_pages; p++) {
        paging_lock_page(p);
    }
    kernel_paging.num_free_pages -= reserved_pages;

    log_msg(LOG_INFO, "Pagination initialisée OK (identity map)");
    return E_OK;
}

void paging_enable(void)
{
    log_msg(LOG_INFO, "Activation de la pagination x86...");

    /* CR3 pointe maintenant vers un répertoire de pages VALIDE et
       entièrement identity-mappé : aucune adresse actuellement
       utilisée par le noyau ne peut générer de #PF. */
    uint32_t pd = (uint32_t)kernel_paging.page_directory;
    __asm__ __volatile__ ("mov %0, %%cr3" : : "r"(pd));

    paging_set_cr0();

    log_msg(LOG_INFO, "Pagination x86 active");
}

void* alloc_page(void)
{
    mutex_lock(&paging_lock);

    for (uint32_t i = 0; i < TOTAL_PAGES; i++) {
        if (paging_is_page_free(i)) {
            paging_lock_page(i);
            kernel_paging.num_free_pages--;

            mutex_unlock(&paging_lock);
            return (void*)(i * PAGE_SIZE);
        }
    }

    mutex_unlock(&paging_lock);
    log_msg(LOG_WARN, "Allocation page échouée : pas de page libre");
    return 0;
}

void* alloc_pages(uint32_t count)
{
    if (count == 0) return 0;

    mutex_lock(&paging_lock);

    uint32_t consecutive = 0;
    uint32_t start_page = 0;

    for (uint32_t i = 0; i < TOTAL_PAGES; i++) {
        if (paging_is_page_free(i)) {
            if (consecutive == 0) {
                start_page = i;
            }
            consecutive++;

            if (consecutive == count) {
                for (uint32_t j = 0; j < count; j++) {
                    paging_lock_page(start_page + j);
                }
                kernel_paging.num_free_pages -= count;

                mutex_unlock(&paging_lock);
                return (void*)(start_page * PAGE_SIZE);
            }
        } else {
            consecutive = 0;
        }
    }

    mutex_unlock(&paging_lock);
    return 0;
}

error_t free_page(void* ptr)
{
    if (!ptr) return E_INVAL;

    uint32_t page_num = (uint32_t)ptr / PAGE_SIZE;

    if (page_num >= TOTAL_PAGES) {
        return E_INVAL;
    }

    mutex_lock(&paging_lock);

    if (paging_is_page_free(page_num)) {
        mutex_unlock(&paging_lock);
        return E_INVAL;
    }

    paging_unlock_page(page_num);
    kernel_paging.num_free_pages++;

    mutex_unlock(&paging_lock);
    return E_OK;
}

error_t free_pages(void* ptr, uint32_t count)
{
    if (!ptr || count == 0) return E_INVAL;

    uint32_t page_num = (uint32_t)ptr / PAGE_SIZE;

    if (page_num + count > TOTAL_PAGES) {
        return E_INVAL;
    }

    mutex_lock(&paging_lock);

    for (uint32_t i = 0; i < count; i++) {
        if (!paging_is_page_free(page_num + i)) {
            paging_unlock_page(page_num + i);
            kernel_paging.num_free_pages++;
        }
    }

    mutex_unlock(&paging_lock);
    return E_OK;
}

error_t map_page(uint32_t virtual, uint32_t physical, uint32_t flags)
{
    if ((virtual % PAGE_SIZE) != 0 || (physical % PAGE_SIZE) != 0) {
        return E_INVAL;
    }

    uint32_t pd_idx = virtual / (PAGE_SIZE * PAGES_PER_TABLE);
    uint32_t pt_idx = (virtual / PAGE_SIZE) % PAGES_PER_TABLE;

    if (pd_idx >= PAGE_TABLES_COUNT || pt_idx >= PAGES_PER_TABLE) {
        return E_INVAL;
    }

    mutex_lock(&paging_lock);

    if (!kernel_paging.page_tables[pd_idx]) {
        void* new_table = alloc_page();
        if (!new_table) {
            mutex_unlock(&paging_lock);
            return E_NOMEM;
        }
        kernel_paging.page_tables[pd_idx] = (uint32_t*)new_table;
        page_directory[pd_idx] = ((uint32_t)new_table) | PAGE_PRESENT | PAGE_WRITE;
    }

    uint32_t* pt = kernel_paging.page_tables[pd_idx];
    pt[pt_idx] = (physical & 0xFFFFF000) | (flags & 0xFFF);

    paging_flush_tlb();

    mutex_unlock(&paging_lock);
    return E_OK;
}

error_t unmap_page(uint32_t virtual)
{
    if ((virtual % PAGE_SIZE) != 0) {
        return E_INVAL;
    }

    uint32_t pd_idx = virtual / (PAGE_SIZE * PAGES_PER_TABLE);
    uint32_t pt_idx = (virtual / PAGE_SIZE) % PAGES_PER_TABLE;

    if (pd_idx >= PAGE_TABLES_COUNT || pt_idx >= PAGES_PER_TABLE) {
        return E_INVAL;
    }

    mutex_lock(&paging_lock);

    if (kernel_paging.page_tables[pd_idx]) {
        uint32_t* pt = kernel_paging.page_tables[pd_idx];
        pt[pt_idx] = 0;
        paging_flush_tlb();
    }

    mutex_unlock(&paging_lock);
    return E_OK;
}

uint32_t get_physical_addr(uint32_t virtual)
{
    uint32_t pd_idx = virtual / (PAGE_SIZE * PAGES_PER_TABLE);
    uint32_t pt_idx = (virtual / PAGE_SIZE) % PAGES_PER_TABLE;

    if (pd_idx >= PAGE_TABLES_COUNT || pt_idx >= PAGES_PER_TABLE) {
        return 0;
    }

    if (!kernel_paging.page_tables[pd_idx]) {
        return 0;
    }

    uint32_t* pt = kernel_paging.page_tables[pd_idx];
    uint32_t entry = pt[pt_idx];

    if (!(entry & PAGE_PRESENT)) {
        return 0;
    }

    return (entry & 0xFFFFF000) + (virtual & 0xFFF);
}

uint32_t paging_get_free_pages(void)
{
    mutex_lock(&paging_lock);
    uint32_t free = kernel_paging.num_free_pages;
    mutex_unlock(&paging_lock);
    return free;
}

uint32_t paging_get_used_pages(void)
{
    mutex_lock(&paging_lock);
    uint32_t used = kernel_paging.num_pages - kernel_paging.num_free_pages;
    mutex_unlock(&paging_lock);
    return used;
}

void* alloc_page_user(void)
{
    void* page = alloc_page();
    if (page) {
        map_page((uint32_t)page, (uint32_t)page,
                PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }
    return page;
}

void* alloc_page_kernel(void)
{
    void* page = alloc_page();
    if (page) {
        map_page((uint32_t)page, (uint32_t)page,
                PAGE_PRESENT | PAGE_WRITE);
    }
    return page;
}
