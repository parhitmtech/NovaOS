#pragma once
/* pit.h - 8254 Programmable Interval Timer driver
 * Programs the PIT to fire IRQ0 at a fixed frequency. This is your first real hardware timer interrupt - the foundation for
 * preemptive scheduling later (a scheduler needs a periodic tick to know when to switch tasks).
 */
#include <stdint.h>

#define PIT_FREQUENCY 1193182u /* the PIT's fixed input clock, in Hz - not configurable */

void pit_init(uint32_t frequency_hz);

/* Incremented once per timer tick by the IRQ0 handler. Useful for crude uptime tracking and, later, scheduling decisions. */
extern volatile uint64_t pit_ticks;
