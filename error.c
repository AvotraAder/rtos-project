#include "error.h"

const char* error_to_string(error_t err)
{
    switch (err) {
        case E_OK:        return "OK (succès)";
        case E_NOMEM:     return "NOMEM (pas de mémoire)";
        case E_INVAL:     return "INVAL (argument invalide)";
        case E_BUSY:      return "BUSY (ressource occupée)";
        case E_TIMEOUT:   return "TIMEOUT (délai expiré)";
        case E_PERM:      return "PERM (permission refusée)";
        case E_NOENT:     return "NOENT (non trouvé)";
        case E_EXIST:     return "EXIST (existe déjà)";
        case E_BADF:      return "BADF (mauvais descripteur)";
        case E_NOSPACE:   return "NOSPACE (espace insuffisant)";
        case E_DEADLK:    return "DEADLK (deadlock détecté)";
        case E_UNKNOWN:   return "UNKNOWN (erreur inconnue)";
        default:          return "? (erreur non définie)";
    }
}
