; isr.asm - exception ISR stubs for NovaOS
; Vectors 0-19 cover the CPU exceptions we care aout right now.
; Some push an error code automatically (per Intel SDM Vol 3, ch 6. 15);
; for the ones that don't, we push  a dummy 0 so the stack layout is
; identical for every vector before isr_common_stub runs.

bits 64
section .text
extern isr_handler

; %1 = vector number, no CPU-pushed error code
%macro ISR_NOERR 1
global isr%1
isr%1:
    push qword 0	; dummy error code
    push qword %1	; vector number
    jmp isr_common_stub
%endmacro

; %1 = vector number, CPU pushes an error code for this one
%macro ISR_ERR 1
global isr%1
isr%1:
    push qword %1	; vector number (error code already pushed by CPU)
    jmp isr_common_stub
%endmacro

ISR_NOERR 0  ; #DE  Divide error
ISR_NOERR 1  ; #DB  Debug
ISR_NOERR 2  ;      NMI
ISR_NOERR 3  ; #BP  BreakPoint
ISR_NOERR 4  ; #OF  Overflow
ISR_NOERR 5  ; #BR  Bound range exceeded
ISR_NOERR 6  ; #UD  Invalid opcode
ISR_NOERR 7  ; #NM  Device not available
ISR_ERR   8  ; #DF  Double fault
ISR_NOERR 9  ;      Reserved (legacy coprocessor segment overrun)
ISR_ERR   10 ; #TS  Invalid TSS
ISR_ERR   11 ; #NP  Segment not present
ISR_ERR   12 ; #SS  Stack-segment fault
ISR_ERR   13 ; #GP  General protection fault
ISR_ERR   14 ; #PF  Page fault
ISR_NOERR 15 ;      Reserved
ISR_NOERR 16 ; #MF  x87 FP excetion
ISR_ERR   17 ; #AC  Alignment check
ISR_NOERR 18 ; #MC  Machine check
ISR_NOERR 19 ; #XF  SIMD FP exception

; ── Common stub ────────────────────────────────────────────────
; Stack on entry (top to bottom): vector, error_code, then the 
; iretq frame the CPU pushed: RIP, CS, RFLAGS, RSP, SS.
isr_common_stub:
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
    call isr_handler

    ; isr_handler does not return for fatal exceptions (it halts),
    ; but pop everyting back in case it ever does for recoverable ones.
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

    add rsp, 16		; discard vector + error_code
    iretq

; ── idt_load ────────────────────────────────────────────────────
; void idt_load(void* idt_pointer_struct)
global idt_load
idt_load:
    lidt [rdi]
    ret
