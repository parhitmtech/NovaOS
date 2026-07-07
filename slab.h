#pragma once
/*
 * slab.h - tensor-aware slab allocator for NovaOS
 * 3-level design
 *	Level 1 - metadata (tensor headers, grad nodes): 64/128/256 bytes, 16-byte aligned
 *	Level 2 - small tensor data: 1KB/4KB/16KB/64KB, 64-byte aligned (cache line)
 *	Level 3 - large tensor data (>64KB): direct PMM, page-aligned, no slab overhead
 *
 * Why a slab on top of the heap?
 * kmalloc does first-fit linear scan - O(n) and fragments badly under repeated same-size alloc/free cycles (exactly what tensor 
	ops do/. The slab pre-carves pages into fixed-size objects and maintains per-size free lists, giving O(1) alloc/free with 
	with zero fragmentation for the common cases. 
 */
#include <stdint.h>
#include <stddef.h>

/* ── Alignment constants ─────────────────────────────────── */
#define SLAB_META_ALIGN  16  /* for tensor headers, grad nodes */
#define SLAB_DATA_ALIGN  64  /* cache line - required for SIMD tensor ops */

/* ── Size class boundaries ───────────────────────────────── */
#define SLAB_META_MAX    256        /* above this = not a metadata object */
#define SLAB_DATA_MAX    (64*1024)  /* above this = large tensor, bypass slab */

/* ── Public API ──────────────────────────────────────────── */
void slab_init(void);

/* 
 * slab_alloc_meta(size) - allocate tensor metadata (header, grad node, etc.)
 * Returns 16-byte aligned pointer size must be <= 256 bytes.
 */
void* slab_alloc_meta(size_t size);

/*
 * slab_alloc_tensor(bytes) - allocate tensor data buffer
 * Returns 64-byte aligned pointer
 * bytes <= 64KB: served from level 2 slab.
 * bytes > 64KB: served from level 3 (direct PMM, page-aligned).
 */
void* slab_alloc_tensor(size_t bytes);

/* 
 * slab_free(ptr) - free any pointer returned by slab_alloc_meta or slab_alloc_tensor. Automatically determines which slab.level
   it came from.
 */
void slab_free(void* ptr);

/* Debug: print per-size stats to VGA */
void slab_print_stats(void);
