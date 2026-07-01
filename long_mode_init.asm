; long_mode_init.asm — sets up 64-bit segments and calls the C kernel

bits 64
section .text
global long_mode_start
extern kernel_main

long_mode_start:
    ; clear out the remaining 32-bit segment registers
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call kernel_main

    ; if kernel_main ever returns, hang
    hlt
