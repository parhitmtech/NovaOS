#pragma once
/* 
 * pmm.h - Physical Memory Manager
 * Tracks which 4kb physical pages are free using a bitmap:
 * 1 bit per page, 0 = free, 1 = used.

 * Why a bitmap? Simple, low overhead, easy to reason about.
 * Downside: O(n) allocation scan - fine for now, worth noting as something to optimize (buddy allocator) once scheduling is in 
 * place.
 * Why 4KB pages? It's the x86-64base page size. Everything above (huge pages, slabs, tensors) is built on top of this granularity
 */

#include <stdint.h>
#include <stddef.h>
#include "multiboot2.h"

#define PMM_PAGE_SIZE 4096 /* 4kB */

/* Initialize the PMM from the Multiboot2 memory map
 * Marks all pages as used first, then marks usable regions free.
 * then re-marks kernel pages as used so we never hand them out. */
void pmm_init(mb2_info_t* mb2_info);

/* Allocate one physical page. Returns physical address, or 0 on failure.
 * Returned page is guaranteed to be zeroed (important for page tables later). */
uint64_t pmm_alloc_page(void);

/* Free a phtysical page. Passing an address not previously allocated
 * is a kernel bug - no silent forgiveness. */
void pmm_free_page(uint64_t addr);

/* Debug: print how many pages are free  vs total to VGA */
void pmm_print_stats(void);
