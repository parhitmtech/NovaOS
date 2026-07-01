/* idt.c - builds and loads the IDT for vectors 0-19 */
#include "idt.h"

static idt_entry_t idt[256];
static idt_pointer_t idt_ptr;

/* Stub entry points defined in isr.asm, one per vector */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void);

#define KERNEL_CODE_SELECTOR 0x08

/* Not static - irq.c installs IRQ gates (32-47) into this same table via the extern declaration in irq.c. idt.c owns
 * the table; irq.c just writes into it through this one shared entry point. */
void idt_install_gate(uint8_t vector, void (*handler)(void)) {
	uint64_t addr = (uint64_t) handler;
	idt[vector].offset_low = addr & 0xFFFF;
	idt[vector].selector = KERNEL_CODE_SELECTOR;
	idt[vector].ist = 0;
	/* present=1, DPL=00, type=0xE (64-bit interrupt gate) -> 0x8E */
	idt[vector].type_attr = 0x8E;
	idt[vector].offset_mid = (addr >> 16) & 0xFFFF;
	idt[vector].offset_high = (addr >> 32) & 0xFFFFFFFF;
	idt[vector].reserved = 0;
}

void idt_init(void) {
    idt_install_gate(0,  isr0);
    idt_install_gate(1,  isr1);
    idt_install_gate(2,  isr2);
    idt_install_gate(3,  isr3);
    idt_install_gate(4,  isr4);
    idt_install_gate(5,  isr5);
    idt_install_gate(6,  isr6);
    idt_install_gate(7,  isr7);
    idt_install_gate(8,  isr8);
    idt_install_gate(9,  isr9);
    idt_install_gate(10, isr10);
    idt_install_gate(11, isr11);
    idt_install_gate(12, isr12);
    idt_install_gate(13, isr13);
    idt_install_gate(14, isr14);
    idt_install_gate(15, isr15);
    idt_install_gate(16, isr16);
    idt_install_gate(17, isr17);
    idt_install_gate(18, isr18);
    idt_install_gate(19, isr19);

    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint64_t) &idt;

    idt_load(&idt_ptr);
}
