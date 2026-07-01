; irq.asm - hardware IRQ stubs (vectors 32-47, after PIC remap)
;
; Same shape as isr.asm's exception stubs, but for hardware interrupts.
; Kept in a separate file from isr.asm because conceptually these are a
; different category (hardware devices vs CPU faults) even though the
; mechanics (push vector, jump to common stub, save/restore registers)
; are identical - separating them makes it obvious at a glance which
; vectors are CPU-defined (0-31) vs hardware - defined (32-47).

bits 64
section .text
extern irq_dispatch

%macro IRQ_STUB 2 ; %1 = vector number, %2 = IRQ line number (0-15)
global irq%2
irq%2:
	push qword 0	; dummy error code, hardware IRQs don't have one
	push qword %1	; vector number
	jmp irq_common_stub
%endmacro

IRQ_STUB 32, 0   ; PIT timer
IRQ_STUB 33, 1   ; Keyboard
IRQ_STUB 34, 2   ; Cascade (used internally by PIC, never fires directly)
IRQ_STUB 35, 3   ; COM2
IRQ_STUB 36, 4   ; COM1
IRQ_STUB 37, 5   ; LPT2
IRQ_STUB 38, 6   ; Floppy disk
IRQ_STUB 39, 7   ; LPT1 / spurious
IRQ_STUB 40, 8   ; RTC
IRQ_STUB 41, 9   ; Free
IRQ_STUB 42, 10  ; Free
IRQ_STUB 43, 11  ; Free
IRQ_STUB 44, 12  ; PS/2 mouse
IRQ_STUB 45, 13  ; FPU
IRQ_STUB 46, 14  ; Primary ATA
IRQ_STUB 47, 15  ; Secondary ATA

; ── Common stub ────────────────────────────────────────────────
; Same register-saving shape as isr_common_stub in isr.asm.
irq_common_stub:
	push rax
	push rbx
	push rcx
	push rdx
	push rsi
	push rdi
	push rbp
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15

	mov rdi, rsp	; pass pointer to saved regs struct as arg 1
	call irq_dispatch

	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rbp
	pop rdi
	pop rsi
	pop rdx
	pop rcx
	pop rbx
	pop rax

	add rsp, 16	; discard vector + error_code
	iretq
