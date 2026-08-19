#ifndef LOGGING_H
#define LOGGING_H

#include <stdint.h>
#include <stddef.h>

/* ──────────────────────────────────────────────────────────────────
   Niveaux de log
   ────────────────────────────────────────────────────────────────── */

typedef enum {
    LOG_DEBUG      = 0,    /* [DEBUG]    - Informations de débogage */
    LOG_INFO       = 1,    /* [INFO]     - Informations générales */
    LOG_WARN       = 2,    /* [WARN]     - Avertissements */
    LOG_ERROR      = 3,    /* [ERROR]    - Erreurs */
    LOG_CRITICAL   = 4     /* [CRITICAL] - Erreurs critiques */
} log_level_t;

/* ──────────────────────────────────────────────────────────────────
   Interface de logging
   ────────────────────────────────────────────────────────────────── */

void log_init(void);
void log_msg(log_level_t level, const char* format, ...);
void log_set_level(log_level_t level);
log_level_t log_get_level(void);

/* ──────────────────────────────────────────────────────────────────
   Dump du buffer de logs
   ────────────────────────────────────────────────────────────────── */

void log_dump(void (*print_fn)(const char*));
void log_clear(void);

/* ──────────────────────────────────────────────────────────────────
   Raccourcis pratiques
   ────────────────────────────────────────────────────────────────── */

#define log_debug(msg)      log_msg(LOG_DEBUG, msg)
#define log_info(msg)       log_msg(LOG_INFO, msg)
#define log_warn(msg)       log_msg(LOG_WARN, msg)
#define log_error(msg)      log_msg(LOG_ERROR, msg)
#define log_critical(msg)   log_msg(LOG_CRITICAL, msg)

#endif /* LOGGING_H */
