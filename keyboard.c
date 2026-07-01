/* keyboard.c - PS/2 keyboard scancode handling */
#include "keyboard.h"
#include "io.h"
#include "pic.h"

#define KEYBOARD_DATA_PORT 0x60

/* Scan code Set 1, unshifted, US Qwerty. Index = scancode.
 * 0 means "no printable ASCII for this scancode" (modifier keys, etc).
 * Scancode >= 0x80 are key-RELEASE events (set bit = press scancode | 0x80),
 * we only translate key-press scancodes here (< 0x80). */

static const char scancode_table[128] = {
	0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
	'\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
	0, /* left ctrl */
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ':', '\'', '`',
	0, /* left shift */
	'\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
	0, /* right shift */
	'*',
	0, /* left alt */
	' ', /* space */
	0, /* caps lock */
	/* F1-F10, num lock, scroll lock, and beyond: unmapped for now */
};

char keyboard_scancode_to_ascii(uint8_t scancode) {
	if (scancode & 0x80) {
		return 0; /* key release event, not a press */
	}
	if (scancode >= sizeof(scancode_table)) {
		return 0;
	}
	return scancode_table[scancode];
}

void keyboard_handle_interrupt(void) {
	uint8_t scancode = inb(KEYBOARD_DATA_PORT);
	char c = keyboard_scancode_to_ascii(scancode);

	if (c != 0) {
		extern void vga_putchar(char c, int fg); /* forward decl, see note below */
		/* NOTE: vga_putchar's real signature takes enum vga_color, not int.
		 * THis forward decl is intentionally loose to avoid a circular #include between keyboard.c and kernel.c
                 * Fix properly by moving vga_putchar's declaration into a shared header (e.g. vga.h) once you split kernel.c into
		 separate driver files -  flagged here as a known rough edge, not silently swept under the rug. */
		vga_putchar(c, 7); /* 7 = VGA_COLOR_LIGHT_GREY */
	}

	pic_send_eoi(1);
}
