#ifndef PIPE_H
#define PIPE_H

#include <stdint.h>
#include "error.h"
#include "mutex.h"
#include "semaphore.h"

/* ──────────────────────────────────────────────────────────────────
   Constantes Pipes
   ────────────────────────────────────────────────────────────────── */

#define PIPE_BUFFER_SIZE    256
#define MAX_PIPES           16

/* ──────────────────────────────────────────────────────────────────
   Structure Pipe
   ────────────────────────────────────────────────────────────────── */

typedef struct {
    uint32_t buffer[PIPE_BUFFER_SIZE];
    int read_idx;
    int write_idx;
    int count;
    
    int readers;        /* Nombre de lecteurs */
    int writers;        /* Nombre de scripteurs */
    
    mutex_t lock;
    semaphore_t not_empty;  /* Signalé quand buffer not empty */
    semaphore_t not_full;   /* Signalé quand buffer not full */
    
    int active;         /* Le pipe est actif */
} pipe_t;

/* ──────────────────────────────────────────────────────────────────
   API Pipes
   ────────────────────────────────────────────────────────────────── */

/* Créer un pipe */
error_t pipe_create(pipe_t* p);

/* Fermer un pipe */
error_t pipe_close(pipe_t* p);

/* Écrire dans un pipe */
error_t pipe_write(pipe_t* p, uint32_t data);
error_t pipe_write_blocking(pipe_t* p, uint32_t data);

/* Lire d'un pipe */
error_t pipe_read(pipe_t* p, uint32_t* data);
error_t pipe_read_blocking(pipe_t* p, uint32_t* data);

/* Flush du pipe */
error_t pipe_flush(pipe_t* p);

/* Statistiques */
int pipe_get_count(pipe_t* p);
int pipe_is_empty(pipe_t* p);
int pipe_is_full(pipe_t* p);

#endif /* PIPE_H */
