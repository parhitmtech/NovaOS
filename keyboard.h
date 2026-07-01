#pragma once
/* keyboard.h - PS/2 keyboard driver (IRQ1)
 * Reads raw scancodes (Scan Code Set, the PS/2 default) from port 0x60
 * and translates them to ASCII. Handles key-down only for now; key-up events (used for modifier keys like Shift) are detected
   but not yet
 * tracked as held state - that's a deliberate scope cut for this stage.
 */
#include <stdint.h>

void keyboard_handle_interrupt(void);

/* Returns 0 if the scancode didn't map to a printable character
 * (e.g. a key-release event, or an unmapped key). */
char keyboard_scancode_to_ascii(uint8_t scancode);
