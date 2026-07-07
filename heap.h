#pragma once 
/* 
 * heap.h - kernel heap allocator
 * Sits on top of PMM. Gives kmalloc/kfree for sub-page allocations.
 * Returns 16-byte alogned pointers - required for tensor/SIMD ops later.
 * Design: free list with block headers. Simple, auditable, good enough to build the slab allocator on top of.
 */
#include <stdint.h>
#include <stddef.h>

void heap_init(void);

/* Allocate at least 'size' bytes, 16-byte aligned. Returns NULL on failure. */
void* kmalloc(size_t size);

/* Free a pointer returned by kmalloc. Passing anything else is a kernel bug */
void kfree(void* ptr);

/* Debug: print heao stats to VGA */
void heap_print_stats(void);
