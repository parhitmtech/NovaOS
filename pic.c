/* pic.c - 8259 PIC remap + EOI/mask handling */
#include "pic.h"
#include "io.h"

void pic_remap(void) {
	uint8_t mask1 = inb(PIC1_DATA); /* save current masks */
	uint8_t mask2 = inb(PIC2_DATA);

	/* ICW1: start initialization sequence, tell PIC we'll send ICW4 */
	outb(PIC1_COMMAND, 0x11);
	io_wait();
	outb(PIC2_COMMAND, 0x11);
	io_wait();

	/* ICW2: vector offsets */
	outb(PIC1_DATA, PIC1_OFFSET);
	io_wait();
	outb(PIC2_DATA, PIC2_OFFSET);
	io_wait();

	/* ICW3: tell master PIC hardware there's a slave PIC at IRQ2 (0000 0100),
	 * tell slave PIC its cascade identity (0000 0010) */
	outb(PIC1_DATA, 0x04);
	io_wait();
	outb(PIC2_DATA, 0x02);
	io_wait();

	/* ICW4: 8086 mode */
	outb(PIC1_DATA, 0x01);
	io_wait();
	outb(PIC2_DATA, 0x01);
	io_wait();

	/* Restore saved masks instead of unmasking everything blindly */
	outb(PIC1_DATA, mask1);
	outb(PIC2_DATA, mask2);
}

void pic_send_eoi(uint8_t irq) {
	if (irq >= 8) {
		outb(PIC2_COMMAND, 0x20); /* slave PIC needs its own EOI too */
	}
	outb(PIC1_COMMAND, 0x20);
}

void pic_set_mask(uint8_t irq) {
	uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
	uint8_t real_irq = (irq < 8) ? irq : irq - 8;
	uint8_t value = inb(port) | (1 << real_irq);
	outb(port, value);
}

void pic_clear_mask(uint8_t irq) {
	uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
	uint8_t real_irq = (irq < 8) ? irq : irq - 8;
	uint8_t value = inb(port) & ~(1 << real_irq);
	outb(port, value);
}
