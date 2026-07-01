#pragma once
/*
 * idt.h - Interrupt Descriptor Table for NovaOS
 *
 * Covers exception vectors 0-19 only (no PIC/hardware IRQs yet).
 * The goal right now is purely diagnostic: turn a silent triple fault
 * into a printed message with the vector, error code, and faulting RIP.
 */
#include <stdint.h>

/* Must match the push order in isr_common_stub in isr.asm EXACTLY.
 * isr_comomon_stub pushes rax..r15 last, so on the C side the LAST
 * thing pushed is at the LOWEST address, i.e. the FIRST struct field */

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector;
    uint64_t error_code;
    /* CPU-pushed iretq frame */
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t user_rsp;
    uint64_t ss;
} __attribute__((packed)) registers_t;

/* 64-bit IDT gate descriptor (interrupt gate, not call gate) */
typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;	/* bite 0-2 = IST index, rest reserved
(0) */
    uint8_t type_attr;       /* gate type + DPL + present bit */
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_pointer_t;

void idt_init(void);

/* Installs a single gate into the shared IDT table. idt.c owns the table;
 * irq.c uses this to install IRQ gates (32-47) alongside the exception gates (0-19) idt_init() installs. */
void idt_install_gate(uint8_t vector, void (*handler)(void));

/* Implemented in isr.asm - loads the IDT pointer via lidt */
extern void idt_load(idt_pointer_t* idt_ptr);

/* Called from isr_common_stub in isr.asm for every exception */
void isr_handler(registers_t* regs);
