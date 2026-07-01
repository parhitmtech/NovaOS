/* irq.c - wires PIC -> PIT + keyboard together and dispatches IRQs */
#include "irq.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"

/* IRQ gate stubs defined in irq.asm, one per hardware interrupt line */
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

/* Shared seam with idt.c - installs a gate into the single shared IDT
 * table without idt.c needing to know anything about hardware IRQs. */
extern void idt_install_gate(uint8_t vector, void (*handler)(void));

void irq_init(void) {
	pic_remap();

	idt_install_gate(32, irq0);
	idt_install_gate(33, irq1);
	idt_install_gate(34, irq2);
	idt_install_gate(35, irq3);
	idt_install_gate(36, irq4);
	idt_install_gate(37, irq5);
	idt_install_gate(38, irq6);
	idt_install_gate(38, irq6);
    	idt_install_gate(39, irq7);
    	idt_install_gate(40, irq8);
    	idt_install_gate(41, irq9);
    	idt_install_gate(42, irq10);
    	idt_install_gate(43, irq11);
    	idt_install_gate(44, irq12);
    	idt_install_gate(45, irq13);
    	idt_install_gate(46, irq14);
    	idt_install_gate(47, irq15);

	pit_init(100); /* 100 Hz tick - 10ms resolution, reasonable for now */
	/* Everything starts masked (disabled) by pic_remap_preserving the BIOS's existing mask state. Explicitly 
	 * enable only what we handle */
	pic_clear_mask(0); /* timer */
	pic_clear_mask(1); /* keyboard */
}

void irq_dispatch(registers_t* regs) {
	uint8_t irq = (uint8_t) regs->vector - 32;

	switch (irq) {
		case 0: /* PIT timer */
			pit_ticks++;
			pic_send_eoi(0);
			break;
		case 1: /* Keyboard */
			keyboard_handle_interrupt(); /* this also sends its own EOI */
			break;
		default:
			/* unhandled IRQ - still must EOI or the PIC stops delivering
			 * interrupts on this line entirely. */
			pic_send_eoi(irq);
			break;
	}
}
