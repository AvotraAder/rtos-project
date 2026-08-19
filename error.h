#ifndef ERROR_H
#define ERROR_H

#include <stdint.h>

/* ──────────────────────────────────────────────────────────────────
   Codes d'erreur RTOS
   ────────────────────────────────────────────────────────────────── */

typedef enum {
    E_OK           = 0,    /* Succès */
    E_NOMEM        = 1,    /* Pas de mémoire disponible */
    E_INVAL        = 2,    /* Argument invalide */
    E_BUSY         = 3,    /* Ressource occupée */
    E_TIMEOUT      = 4,    /* Timeout expiré */
    E_PERM         = 5,    /* Permission refusée */
    E_NOENT        = 6,    /* Fichier/Ressource non trouvé */
    E_EXIST        = 7,    /* Le fichier existe déjà */
    E_BADF         = 8,    /* Mauvais descripteur */
    E_NOSPACE      = 9,    /* Pas d'espace disque */
    E_DEADLK       = 10,   /* Détection de deadlock */
    E_UNKNOWN      = 11    /* Erreur inconnue */
} error_t;

/* ──────────────────────────────────────────────────────────────────
   Macros utiles
   ────────────────────────────────────────────────────────────────── */

#define CHECK_RET(x) \
    do { \
        error_t _err = (x); \
        if (_err != E_OK) { \
            return _err; \
        } \
    } while(0)

#define CHECK_NULL(ptr, err_code) \
    do { \
        if (!(ptr)) { \
            return (err_code); \
        } \
    } while(0)

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            panic(msg, 0); \
        } \
    } while(0)

#define WARN_IF(cond, msg) \
    do { \
        if (cond) { \
            log_msg(LOG_WARN, msg); \
        } \
    } while(0)

/* ──────────────────────────────────────────────────────────────────
   Conversion erreur -> string
   ────────────────────────────────────────────────────────────────── */

const char* error_to_string(error_t err);

#endif /* ERROR_H */
