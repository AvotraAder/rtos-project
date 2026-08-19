#include "pipe.h"
#include "logging.h"

/* ──────────────────────────────────────────────────────────────────
   Implémentation des pipes
   ────────────────────────────────────────────────────────────────── */

error_t pipe_create(pipe_t* p)
{
    if (!p) {
        return E_INVAL;
    }
    
    p->read_idx = 0;
    p->write_idx = 0;
    p->count = 0;
    p->readers = 1;
    p->writers = 1;
    p->active = 1;
    
    mutex_init(&p->lock);
    sem_init(&p->not_empty, 0, 1);
    sem_init(&p->not_full, 1, 1);
    
    log_msg(LOG_DEBUG, "Pipe créé");
    
    return E_OK;
}

error_t pipe_close(pipe_t* p)
{
    if (!p) {
        return E_INVAL;
    }
    
    mutex_lock(&p->lock);
    p->active = 0;
    mutex_unlock(&p->lock);
    
    log_msg(LOG_DEBUG, "Pipe fermé");
    
    return E_OK;
}

error_t pipe_write(pipe_t* p, uint32_t data)
{
    if (!p || !p->active) {
        return E_BADF;
    }
    
    mutex_lock(&p->lock);
    
    if (p->count >= PIPE_BUFFER_SIZE) {
        mutex_unlock(&p->lock);
        return E_BUSY;  /* Buffer plein (non-bloquant) */
    }
    
    p->buffer[p->write_idx] = data;
    p->write_idx = (p->write_idx + 1) % PIPE_BUFFER_SIZE;
    p->count++;
    
    mutex_unlock(&p->lock);
    
    /* Signaler que le buffer n'est pas vide */
    sem_post(&p->not_empty);
    
    return E_OK;
}

error_t pipe_write_blocking(pipe_t* p, uint32_t data)
{
    if (!p || !p->active) {
        return E_BADF;
    }
    
    /* Attendre que le buffer ne soit pas plein */
    sem_wait(&p->not_full);
    
    return pipe_write(p, data);
}

error_t pipe_read(pipe_t* p, uint32_t* data)
{
    if (!p || !p->active || !data) {
        return E_INVAL;
    }
    
    mutex_lock(&p->lock);
    
    if (p->count == 0) {
        mutex_unlock(&p->lock);
        return E_BUSY;  /* Buffer vide (non-bloquant) */
    }
    
    *data = p->buffer[p->read_idx];
    p->read_idx = (p->read_idx + 1) % PIPE_BUFFER_SIZE;
    p->count--;
    
    mutex_unlock(&p->lock);
    
    /* Signaler que le buffer n'est pas plein */
    sem_post(&p->not_full);
    
    return E_OK;
}

error_t pipe_read_blocking(pipe_t* p, uint32_t* data)
{
    if (!p || !p->active || !data) {
        return E_INVAL;
    }
    
    /* Attendre qu'il y ait des données */
    sem_wait(&p->not_empty);
    
    return pipe_read(p, data);
}

error_t pipe_flush(pipe_t* p)
{
    if (!p) {
        return E_INVAL;
    }
    
    mutex_lock(&p->lock);
    
    p->read_idx = 0;
    p->write_idx = 0;
    p->count = 0;
    
    mutex_unlock(&p->lock);
    
    return E_OK;
}

int pipe_get_count(pipe_t* p)
{
    if (!p) return 0;
    
    mutex_lock(&p->lock);
    int cnt = p->count;
    mutex_unlock(&p->lock);
    
    return cnt;
}

int pipe_is_empty(pipe_t* p)
{
    if (!p) return 1;
    return pipe_get_count(p) == 0;
}

int pipe_is_full(pipe_t* p)
{
    if (!p) return 0;
    return pipe_get_count(p) >= PIPE_BUFFER_SIZE;
}
