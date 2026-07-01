/* pit.c - programs PIT channel 0 to fire at the requested frequency */
#include "pit.h"
#include "io.h"

#define PIT_CHANNEL0_DATA 0x40
#define PIT_COMMAND       0x43

volatile uint64_t pit_ticks = 0;

void pit_init(uint32_t frequency_hz) {
	uint16_t divisor = (uint16_t) (PIT_FREQUENCY / frequency_hz);

	/* 0x36 = channel 0, lobyte/hibyte access mode, mode 3 (square wave),
	 * binary mode (not BCD) */
	outb(PIT_COMMAND, 0x36);

	/* divisor must be sent low byte first, then high byte */
	outb(PIT_CHANNEL0_DATA, (uint8_t) (divisor & 0xFF));
	outb(PIT_CHANNEL0_DATA, (uint8_t) ((divisor >> 8) & 0xFF));
}
