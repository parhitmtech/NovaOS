/* linker.ld — lays out NovaOS kernel sections in memory
 * Loads the kernel at the 1MB mark, which is standard for
 * x86 kernels (below 1MB is reserved for BIOS/legacy stuff).
 */

ENTRY(start)

SECTIONS
{
    . = 1M;

    .boot :
    {
        /* Multiboot header must be early in the binary so GRUB finds it */
        *(.multiboot_header)
    }

    .text :
    {
        *(.text)
    }

    .rodata :
    {
        *(.rodata)
    }

    .data :
    {
        *(.data)
    }

    .bss :
    {
        *(COMMON)
        *(.bss)
    }
}
