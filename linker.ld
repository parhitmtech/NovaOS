/* linker.ld — lays out NovaOS kernel sections in memory
 * Loads the kernel at the 1MB mark, which is standard for
 * x86 kernels (below 1MB is reserved for BIOS/legacy stuff).
 *
 * CHANGED: exports _kernel_start and _kernel_end so the PMM
 * knows exactly which physical pages to protect from allocation.
 */
ENTRY(start)
SECTIONS
{
    . = 1M;

    _kernel_start = .;  /* PMM reads this to know where kernel begins */

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

    _kernel_end = .;    /* PMM reads this to know where kernel ends */
}
