#ifndef SIGNAL_H
#define SIGNAL_H

#include <stdint.h>
#include "error.h"

#define NUM_SIGNALS 32

#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGBUS    7
#define SIGFPE    8
#define SIGKILL   9
#define SIGUSR1   10
#define SIGUSR2   11
#define SIGSEGV   12
#define SIGALRM   13
#define SIGTERM   14
#define SIGSTOP   19  // <-- Fixed: Added SIGSTOP definition

#define SIG_DFL ((signal_handler_t)0)
#define SIG_IGN ((signal_handler_t)1)

typedef void (*signal_handler_t)(int);

typedef struct {
    signal_handler_t handlers[NUM_SIGNALS];
    uint32_t pending_signals;
    uint32_t blocked_signals;
} signal_ctx_t;

error_t signal_init(void);
void signal_init_ctx(signal_ctx_t* ctx);
signal_handler_t signal(int signum, signal_handler_t handler);
error_t kill(uint32_t pid, int signum);
error_t raise(int signum);
error_t sigblock(uint32_t mask);
error_t sigunblock(uint32_t mask);
error_t pause(void);
void signal_dispatch(void);

#endif
