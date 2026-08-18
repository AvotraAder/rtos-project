#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

#define TIMER_MAX_CALLBACKS 8

/* Callback périodique : appelé à chaque tick */
typedef void (*timer_callback_t)(void);

void     init_timer(uint32_t frequency);
uint32_t get_ticks(void);
uint32_t get_uptime_sec(void);

/* Enregistrement de callbacks supplémentaires */
int  timer_register_callback(timer_callback_t cb);
void timer_unregister_callback(timer_callback_t cb);

/* Utilitaire : attente active (spin) en millisecondes */
void timer_wait_ms(uint32_t ms);

#endif /* TIMER_H */
