#ifndef TIMER_H
#define TIMER_H

#include "types.h"

void     timer_init(uint32_t hz);
uint32_t timer_ticks(void);   /* Phase 24: current PIT tick count */

#endif