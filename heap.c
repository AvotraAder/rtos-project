#include "heap.h"
#include "mutex.h"

#define HEAP_START 0x01000000          /* Adresse de début du Tas (16 Mo) */
#define HEAP_SIZE  (1024 * 1024 * 4)   /* Taille du Tas (4 Mo) */

typedef struct header {
    size_t size;
    int is_free;
    struct header* next;
} header_t;

static header_t* heap_first = (header_t*)HEAP_START;
static mutex_t heap_mutex;

void init_heap(void) {
    heap_first->size = HEAP_SIZE - sizeof(header_t);
    heap_first->is_free = 1;
    heap_first->next = 0;
    mutex_init(&heap_mutex);
}

void* kmalloc(size_t size) {
    if (size == 0) return 0;

    /* Alignement sur 4 octets */
    if (size % 4 != 0) {
        size += 4 - (size % 4);
    }

    mutex_lock(&heap_mutex);
    header_t* curr = heap_first;

    while (curr) {
        if (curr->is_free && curr->size >= size) {
            /* Séparation du bloc si l'espace restant est suffisant */
            if (curr->size >= size + sizeof(header_t) + 16) {
                header_t* next_block = (header_t*)((uint8_t*)curr + sizeof(header_t) + size);
                next_block->size = curr->size - size - sizeof(header_t);
                next_block->is_free = 1;
                next_block->next = curr->next;

                curr->size = size;
                curr->next = next_block;
            }
            curr->is_free = 0;
            mutex_unlock(&heap_mutex);
            return (void*)((uint8_t*)curr + sizeof(header_t));
        }
        curr = curr->next;
    }

    mutex_unlock(&heap_mutex);
    return 0; /* Mémoire saturée */
}

void kfree(void* ptr) {
    if (!ptr) return;

    mutex_lock(&heap_mutex);
    header_t* header = (header_t*)((uint8_t*)ptr - sizeof(header_t));
    header->is_free = 1;

    /* Fusion des blocs libres contigus */
    header_t* curr = heap_first;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            curr->size += sizeof(header_t) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
    mutex_unlock(&heap_mutex);
}

size_t heap_get_used(void) {
    mutex_lock(&heap_mutex);
    size_t used = 0;
    header_t* curr = heap_first;
    while (curr) {
        if (!curr->is_free) {
            used += curr->size + sizeof(header_t);
        }
        curr = curr->next;
    }
    mutex_unlock(&heap_mutex);
    return used;
}

size_t heap_get_free(void) {
    return HEAP_SIZE - heap_get_used();
}
