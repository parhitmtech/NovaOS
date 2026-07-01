#pragma once
/* pic.h - 8259 Programmable interrupt Controller driver
 * By default the PIC maps hardware IRQs 0-15 onto interrupt vectors 0-15 which collides head-on with the CPU exception vectors
 * just wired up
 * (divide error = 0, invalid opcode = 6, etc). Without remapping, a timer
 * tick would look identical to a CPU exception. This remaps IRQs to vectors 32-47 instead, clear of the CPU's reserved 0-31 range
 */
#include <stdint.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define PIC1_OFFSET 32 /* IRQ0-7 -> Vectors 32-39 */
#define PIC2_OFFSET 40 /* IRQ8-15 -> Vectors 40-47 */

void pic_remap(void);

/* Every hardware interrupt handler must call this when done, telling the PIC it is safe to send more interrupts.
 * Forgetting this is a classic bug: the PIC just stops delivering interrupts after the first one. */
void pic_send_eoi(uint8_t irq);

/* Optional: mask (disable) or unmask (enable) a specific IRQ line.
*/

void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);
