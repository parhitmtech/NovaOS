#pragma once
/* irq.h - routes hadrware interrupts (vectors 32-47) to device drivers */
#include "idt.h" /* reuses the same registers_t layout as exceptions */

void irq_init(void); /* remaps PIC, installs IRQ gates, starts PIT, unmasks IRQ0+1 */


/* Called from irq_common_stub (irq.asm) for every hardware interrupt.
 * Looks at regs->vector to decide which driver handles it. */

void irq_dispatch(registers_t* regs);
