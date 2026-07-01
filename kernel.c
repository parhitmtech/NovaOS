/* Kernel.c - NovaOS Kernel entry point
 * First C code that runs after long mode is set up.
 * Prints a boot banner direcly to VGA text memory.
 */
#include <stdint.h>
#include <stddef.h>
#include "idt.h"
#include "irq.h"

/* VGA text mode buffer lives at physical address 0xB8000.
 * Each character cell is 2 bytes: [ASCII char][color attribute]
 */
static uint16_t* const VGA_BUFFER = (uint16_t*) 0xB8000;
static const int VGA_WIDTH = 80;
static const int VGA_HEIGHT = 25;
static int cursor_row = 0;
static int cursor_col = 0;

enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_RED = 4,
    VGA_COLOR_WHITE = 15,
};

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg)
{
    return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char c, uint8_t color)
{
    return (uint16_t) c | (uint16_t) color << 8;
}

void vga_clear(void)
{
    uint8_t color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[y * VGA_WIDTH + x] = vga_entry(' ', color);
        }
    }
    cursor_row = 0;
    cursor_col = 0;
}

static void vga_scroll(void) {
    for (int y = 1; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[(y - 1) * VGA_WIDTH + x] = VGA_BUFFER[y * VGA_WIDTH + x];
        }
    }
    uint8_t color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    for (int x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', color);
    }
    cursor_row = VGA_HEIGHT - 1;
}

void vga_putchar(char c, enum vga_color fg) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else {
        uint8_t color = vga_entry_color(fg, VGA_COLOR_BLACK);
        VGA_BUFFER[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(c, color);
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }
    if (cursor_row >= VGA_HEIGHT) {
        vga_scroll();
    }
}

void vga_print(const char* str, enum vga_color fg) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        vga_putchar(str[i], fg);
    }
}

/* Minimal hex printer - no libc available in a freestanding kernel. */
static void vga_print_hex(uint64_t value, enum vga_color fg) {
    char buf[19]; /* "0x" + 16 hex digits + null */
    const char* hex_chars = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; i++) {
        uint8_t nibble = (value >> ((15 - i) * 4)) & 0xF;
        buf[2 + i] = hex_chars[nibble];
    }
    buf[18] = '\0';
    vga_print(buf, fg);
}

/* Exception names for vectors 0-19, per Intel SDM Vol 3, ch 6 */
static const char* exception_name(uint64_t vector) {
    static const char* names[20] = {
        "Divide Error", "Debug", "NMI", "BreakPoint", "Overflow",
        "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
        "Double Fault", "Reserved", "Invalid TSS", "Segment Not Present",
        "Stack-Segment Fault", "General Protection Fault", "Page Fault",
        "Reserved", "x87 FP Exception", "Alignment Check",
    };
    if (vector < 20) return names[vector];
    return "Unknown Exception";
}

/* Called  from isr_common_stub (isr.asm) for every exception.
 * These are all fatal for now - we have no recovery path yet -
 * so we print what happened and halt instead of triple-faulting.
*/
void isr_handler(registers_t* regs) {
    vga_print("\n*** KERNEL PANIC: ", VGA_COLOR_RED);
    vga_print(exception_name(regs->vector), VGA_COLOR_RED);
    vga_print(" ***\n", VGA_COLOR_RED);

    vga_print("Vector: ", VGA_COLOR_WHITE);
    vga_print_hex(regs->vector, VGA_COLOR_LIGHT_GREY);
    vga_print("  Error code: ", VGA_COLOR_WHITE);
    vga_print_hex(regs->error_code, VGA_COLOR_LIGHT_GREY);
    vga_print("\n", VGA_COLOR_WHITE);

    vga_print("Faulting RIP: ", VGA_COLOR_WHITE);
    vga_print_hex(regs->rip, VGA_COLOR_LIGHT_GREY);
    vga_print("\n", VGA_COLOR_WHITE);

    vga_print("RSP: ", VGA_COLOR_WHITE);
    vga_print_hex(regs->user_rsp, VGA_COLOR_LIGHT_GREY);
    vga_print("  CR-state not yet captured\n", VGA_COLOR_WHITE);

    vga_print("\nSystem halted.\n", VGA_COLOR_RED);

    for (;;) {
        __asm__ volatile ("hlt");
    }
}

/* ── Kernel entry point ──────────────────────────────────────── */
void kernel_main(void) {
    	vga_clear();
    	vga_print("NovaOS v0.1\n", VGA_COLOR_GREEN);
    	vga_print("ML-first operating system booting...\n", VGA_COLOR_WHITE);
    	vga_print("\n", VGA_COLOR_WHITE);
    	vga_print("[OK] Long mode initialized\n", VGA_COLOR_LIGHT_GREY);
    	vga_print("[OK] Paging enabled\n", VGA_COLOR_LIGHT_GREY);
    	vga_print("[OK] VGA text driver loaded\n", VGA_COLOR_LIGHT_GREY);
    	idt_init();
    	vga_print("[OK] IDT loaded (exceptions 0-19)\n", VGA_COLOR_LIGHT_GREY);
	irq_init();
	vga_print("[OK] PIC remapped, PIT + Keyboard IRQs installed\n", VGA_COLOR_LIGHT_GREY);
	__asm__ volatile ("sti"); /* enable interrupts - IRQs were prepared masked until now */
	vga_print("[OK] Interrupts enabled\n", VGA_COLOR_LIGHT_GREY);
    	vga_print("\n", VGA_COLOR_WHITE);
    	vga_print("Kernel running. Type something:\n", VGA_COLOR_WHITE);

    /* Sanity check: deliberately trigger #DE (divide by zero) to prove
     * the IDT actually catches it instead of triple-faulting.
     * Remvoe this once you-ve confirmed  it works. */
    /*
    volatile int a = 1, b = 0;
    volatile int c = a / b;
    (void) c;
    */

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
