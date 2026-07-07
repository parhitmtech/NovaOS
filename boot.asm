; boot.asm — Multiboot2 entry point for NovaOS
; Switches from 32-bit protected mode (where GRUB leaves us)
; into 64-bit long mode, then jumps to the C kernel.

bits 32

section .multiboot_header
header_start:
    dd 0xe85250d6                ; multiboot2 magic number
    dd 0                         ; architecture 0 = i386 protected mode
    dd header_end - header_start ; header length
    dd 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start)) ; checksum

    ; end tag
    dw 0
    dw 0
    dd 8
header_end:

section .bss
align 4096
p4_table:
    resb 4096
p3_table:
    resb 4096
p2_table:
    resb 4096
stack_bottom:
    resb 16384      ; 16 KB stack
stack_top:
global multiboot_ptr
multiboot_ptr:
    resd 1	; 4 bytes to store the 32-bit Multiboot2 pointer
section .text
bits 32
global start
extern long_mode_start

start:
    cli
    mov esp, stack_top
    mov [multiboot_ptr], ebx	; save BEFORE any call clobbers ebx
    call check_multiboot
    call check_cpuid
    call check_long_mode

    call set_up_page_tables
    call enable_paging

    ; load the 64-bit GDT
    lgdt [gdt64.pointer]

    ; jump into long mode
    jmp gdt64.code_segment:long_mode_start

    hlt

; ── Sanity checks ────────────────────────────────────────────
check_multiboot:
    cmp eax, 0x36d76289
    jne .no_multiboot
    ret
.no_multiboot:
    mov al, "0"
    jmp error

check_cpuid:
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    cmp eax, ecx
    je .no_cpuid
    ret
.no_cpuid:
    mov al, "1"
    jmp error

check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode
    ret
.no_long_mode:
    mov al, "2"
    jmp error

; ── Page tables for long mode ────────────────────────────────
set_up_page_tables:
    ; p4_table[0] -> p3_table
    mov eax, p3_table
    or eax, 0b11        ; present + writable
    mov [p4_table], eax

    ; p3_table[0] -> p2_table
    mov eax, p2_table
    or eax, 0b11
    mov [p3_table], eax

    ; map first 1GB using 2MB huge pages
    mov ecx, 0
.map_p2_table:
    mov eax, 0x200000   ; 2MB
    mul ecx
    or eax, 0b10000011  ; present + writable + huge page
    mov [p2_table + ecx * 8], eax

    inc ecx
    cmp ecx, 512
    jne .map_p2_table

    ret

enable_paging:
    mov eax, p4_table
    mov cr3, eax

    mov eax, cr4
    or eax, 1 << 5      ; PAE
    mov cr4, eax

    mov ecx, 0xC0000080 ; EFER MSR
    rdmsr
    or eax, 1 << 8      ; long mode bit
    wrmsr

    mov eax, cr0
    or eax, 1 << 31     ; enable paging
    or eax, 1 << 0      ; enable protected mode
    mov cr0, eax

    ret

error:
    mov dword [0xb8000], 0x4f524f45
    mov dword [0xb8004], 0x4f3a4f52
    mov dword [0xb8008], 0x4f204f20
    mov byte  [0xb800a], al
    hlt

; ── 64-bit GDT ────────────────────────────────────────────────
section .rodata
gdt64:
    dq 0 ; zero entry
.code_segment: equ $ - gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53) ; code segment
.pointer:
    dw $ - gdt64 - 1
    dq gdt64
