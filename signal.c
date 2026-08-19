#include "signal.h"
#include "logging.h"
#include "process.h"
#include "mutex.h"

/* ──────────────────────────────────────────────────────────────────
   Variables globales
   ────────────────────────────────────────────────────────────────── */

static signal_ctx_t signal_contexts[16];  /* Max 16 processus */
static mutex_t signal_lock;

static const char* signal_names[NUM_SIGNALS] = {
    "INVALID",  "SIGHUP",   "SIGINT",   "SIGQUIT",  "SIGILL",
    "SIGTRAP",  "SIGABRT",  "SIGBUS",   "SIGFPE",   "SIGKILL",
    "SIGUSR1",  "SIGUSR2",  "SIGSEGV",  "SIGALRM",  "SIGTERM",
    "SIG15",    "SIG16",    "SIG17",    "SIG18",    "SIG19",
    "SIG20",    "SIG21",    "SIG22",    "SIG23",    "SIG24",
    "SIG25",    "SIG26",    "SIG27",    "SIG28",    "SIG29",
    "SIG30",    "SIG31"
};

/* ──────────────────────────────────────────────────────────────────
   Handlers par défaut
   ────────────────────────────────────────────────────────────────── */

static void default_signal_handler(int signum)
{
    const char* name = (signum < NUM_SIGNALS) ? signal_names[signum] : "UNKNOWN";
    log_msg(LOG_INFO, "Signal reçu");
    (void)name; /* Évite l'avertissement d'unused variable */
    
    /* Terminer le processus par défaut */
    if (signum == SIGTERM || signum == SIGKILL) {
        process_exit(process_current()->pid);
    }
}

/* ──────────────────────────────────────────────────────────────────
   API publique
   ────────────────────────────────────────────────────────────────── */

error_t signal_init(void)
{
    log_msg(LOG_INFO, "Initialisation du système de signaux");
    
    mutex_init(&signal_lock);
    
    for (int i = 0; i < 16; i++) {
        signal_init_ctx(&signal_contexts[i]);
    }
    
    return E_OK;
}

void signal_init_ctx(signal_ctx_t* ctx)
{
    for (int i = 0; i < NUM_SIGNALS; i++) {
        ctx->handlers[i] = SIG_DFL;
    }
    ctx->pending_signals = 0;
    ctx->blocked_signals = 0;
}

signal_handler_t signal(int signum, signal_handler_t handler)
{
    if (signum < 0 || signum >= NUM_SIGNALS) {
        return SIG_DFL;
    }
    
    if (signum == SIGKILL || signum == SIGSTOP) {
        /* Ces signaux ne peuvent pas être changés */
        return SIG_DFL;
    }
    
    process_t* current = process_current();
    if (!current) return SIG_DFL;
    
    uint32_t pid = current->pid;
    if (pid >= 16) return SIG_DFL;
    
    mutex_lock(&signal_lock);
    
    signal_handler_t old = signal_contexts[pid].handlers[signum];
    signal_contexts[pid].handlers[signum] = handler ? handler : SIG_DFL;
    
    mutex_unlock(&signal_lock);
    
    return old;
}

error_t kill(uint32_t pid, int signum)
{
    if (signum < 0 || signum >= NUM_SIGNALS) {
        return E_INVAL;
    }
    
    if (pid >= 16) {
        return E_INVAL;
    }
    
    process_t* target = process_get(pid);
    if (!target) {
        return E_NOENT;
    }
    
    mutex_lock(&signal_lock);
    
    /* Marquer le signal comme en attente */
    signal_contexts[pid].pending_signals |= (1 << signum);
    
    log_msg(LOG_DEBUG, "Signal envoyé");
    
    mutex_unlock(&signal_lock);
    
    return E_OK;
}

error_t raise(int signum)
{
    process_t* current = process_current();
    if (!current) return E_INVAL;
    
    return kill(current->pid, signum);
}

error_t sigblock(uint32_t mask)
{
    process_t* current = process_current();
    if (!current || current->pid >= 16) {
        return E_INVAL;
    }
    
    mutex_lock(&signal_lock);
    signal_contexts[current->pid].blocked_signals |= mask;
    mutex_unlock(&signal_lock);
    
    return E_OK;
}

error_t sigunblock(uint32_t mask)
{
    process_t* current = process_current();
    if (!current || current->pid >= 16) {
        return E_INVAL;
    }
    
    mutex_lock(&signal_lock);
    signal_contexts[current->pid].blocked_signals &= ~mask;
    mutex_unlock(&signal_lock);
    
    return E_OK;
}

error_t pause(void)
{
    process_t* current = process_current();
    if (!current) return E_INVAL;
    
    while (!(signal_contexts[current->pid].pending_signals & 
             ~signal_contexts[current->pid].blocked_signals)) {
        __asm__ __volatile__ ("hlt");
    }
    
    return E_OK;
}

void signal_dispatch(void)
{
    process_t* current = process_current();
    if (!current || current->pid >= 16) {
        return;
    }
    
    mutex_lock(&signal_lock);
    
    signal_ctx_t* ctx = &signal_contexts[current->pid];
    uint32_t pending = ctx->pending_signals & ~ctx->blocked_signals;
    
    for (int sig = 0; sig < NUM_SIGNALS; sig++) {
        if (pending & (1 << sig)) {
            signal_handler_t handler = ctx->handlers[sig];
            
            ctx->pending_signals &= ~(1 << sig);
            
            mutex_unlock(&signal_lock);
            
            if (handler && handler != SIG_IGN) {
                handler(sig);
            } else if (handler == SIG_DFL) {
                default_signal_handler(sig);
            }
            
            mutex_lock(&signal_lock);
        }
    }
    
    mutex_unlock(&signal_lock);
}
