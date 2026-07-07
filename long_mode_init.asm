; long_mode_init.asm — sets up 64-bit segments and calls the C kernel

bits 64
section .text
global long_mode_start
extern kernel_main
extern multiboot_ptr

long_mode_start:
    ; clear out the remaining 32-bit segment registers
    cli
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; rbx still holds the MultiBoot2 info pointer from GRUB (passed in ebx, 
    ; zero-extended to rbx automatically when written as 32-bit in long mode).
    ; System V AMD64 ABI: first argument goes in rdi.
    xor rdi, rdi
    mov edi, [multiboot_ptr]
    call kernel_main

    ; if kernel_main ever returns, hang
    hlt
