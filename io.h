#pragma once
/* io.h - raw x86 part I/O primitives, shared by PIC/PIT/Keyboard drivers */
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
	__asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
	uint8_t ret;
	__asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

/* Small delay for old hardware that can't keep up with back-to-back I/O.
 * Writing to port 0x80 is a traditional unused POST-code port, safe to write to. */
static inline void io_wait(void) {
	outb(0x80, 0);
}
