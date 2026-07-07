/* heap.c - free list heap allocator for NovaOS kernel
 * Layout of every allocation:
 * [ block_header_t | <padding to 16-byte align data> | data...]
 * The pointer returned to the caller points at 'data', not at the header.
 * kfree() walks back by sizeof(block_header_t) to find the header
 * Coalescing: on every kfree(), we scan forward and merge adjacent free blocks. O(n) but simple and correct. Revisit if 
 * fragmentation becomes a real problem after the slab allocator is in place.
 */
#include "heap.h"
#include "pmm.h"

/* ── Block header ─────────────────────────────────────────── */
/* Sits immediately before every allocation (free or used).
 * size = total size of the block including this header.
 * next = next block in the list (by address, not a pointer chain). */
typedef struct block_header {
	size_t size;	/* total block size including header */
	uint32_t free;	/* 1 = free, 0 = used */
	uint32_t magic;	/* 0xDEADBEEF = valid header, catches corruption */
	struct block_header* next; /* next block in list */
} block_header_t;

#define HEAP_MAGIC   0xDEADBEEF
#define HEADER_SIZE  sizeof(block_header_t)

/* ── Alignment ────────────────────────────────────────────── */
/* Round up to the nearest multiple of 16.
 * Required for SIMD/tensor ops that use aligned loads. */
#define ALIGN16(x) (((x) + 15) & ~(size_t)15)

/* ── Heap state ───────────────────────────────────────────── */
static block_header_t* heap_start = 0;
static block_header_t* heap_end = 0;

static size_t total_allocated = 0;
static size_t total_free = 0;

/* ── Internal: expand heap by one PMM page ────────────────── */
static block_header_t* heap_expand(void) {
	uint64_t page = pmm_alloc_page();
	if (!page) return 0; /* out of physical memory */

	block_header_t* block = (block_header_t*)(uint64_t) page;
	block->size = PMM_PAGE_SIZE;
	block->free = 1;
	block->magic = HEAP_MAGIC;
	block->next = 0;

	total_free += PMM_PAGE_SIZE;

	if (!heap_start) {
		heap_start = block;
		heap_end = block;
	} else {
		heap_end->next = block;
		heap_end = block;
	}
	return block;
}

/* ── Internal: coalesce adjacent free blocks ──────────────── */
static void coalesce(void) {
	block_header_t* curr = heap_start;
	while (curr && curr->next) {
		if (curr->free && curr->next->free) {
			/* merge curr and curr->next into one larger free block */
			curr->size += curr->next->size;
			curr->next = curr->next->next;
			/* don't advance - keep checking if the new next is also free */
		} else {
			curr = curr->next;
		}
	}
}

/* ── Public API ───────────────────────────────────────────── */
void heap_init(void) {
	heap_start = 0;
	heap_end = 0;
	total_allocated = 0;
	total_free = 0;
	/* Expand immediately so the first kmalloc doesn't have to */
	heap_expand();
}

void* kmalloc(size_t size) {
	if (size == 0) return 0;

	/* Round-up size to 16-byte alignment so the returned pointer is always assigned regardless of header size */
	size_t aligned_size = ALIGN16(size);
	size_t needed = HEADER_SIZE + aligned_size;

	/* Walk free list looking for a big enough block */
	block_header_t* curr = heap_start;
	while (curr) {
		if (curr->magic != HEAP_MAGIC) {
			/* Heap corruption - someone wrote past their allocation */
			return 0;
		}

		if (curr->free && curr->size >= needed) {
			/* Split the block if there's enough space left over a useful block (header + at least 16 bytes of data) 
			*/
			if (curr->size >= needed + HEADER_SIZE + 16) {
				block_header_t* remainder = (block_header_t*) ((uint8_t*) curr + needed);
				remainder->size = curr->size - needed;
				remainder->free = 1;
				remainder->magic = HEAP_MAGIC;
				remainder->next = curr->next;

				curr->size = needed;
			}

			curr->free = 0;
			total_allocated += curr->size;
			total_free -= curr->size;

			/* Return pointer to data area (past the header) */
			return (void*) ((uint8_t*) curr + HEADER_SIZE);
		}

		curr = curr->next;
	}

	/* No suitable block found - get more memory from PMM and retry */
	size_t expanded = 0;
	while (expanded * PMM_PAGE_SIZE < needed) {

		if (!heap_expand()) return 0;
		expanded++;
	}
	/* Now entry the allocation - don't recurse, just scan again */
	block_header_t* curr2 = heap_start;
	while (curr2) {
		if (curr2->free && curr2->size >= needed) {
			if (curr2->size >= needed + HEADER_SIZE + 16) {
				block_header_t* remainder = (block_header_t*) ((uint8_t*) curr2 + needed);
				remainder->size = curr2->size - needed;
				remainder->free = 1;
				remainder->magic = HEAP_MAGIC;
				remainder->next = curr2->next;
				curr2->size = needed;
				curr2->next = remainder;
			}
			curr2->free = 0;
			total_allocated += curr2->size;
			total_free -= curr2->size;
			return (void*) ((uint8_t*) curr2 + HEADER_SIZE);
		}
		curr2 = curr2->next;
	}
	return 0;
}

void kfree(void* ptr) {
	if (!ptr) return;

	block_header_t* header = (block_header_t*) ((uint8_t*) ptr - HEADER_SIZE);

	if (header->magic != HEAP_MAGIC) {
		/* Double free or bad pointer - kernel bug, halt */
		__asm__ volatile ("hlt");
		return;
	}

	if (header->free) {
		/* Double free detached */
		__asm__ volatile ("hlt");
		return;
	}

	header->free = 1;
	total_free += header->size;
	total_allocated -= header->size;

	coalesce();
}

/* Forward decls - same pattern as pmm.c, clean up when vga.h exists */
extern void vga_print(const char* str, int fg);

static void print_dec(uint64_t n) {
	char buf[20];
	int i = 0;
	if (n == 0) { vga_print("0", 7); return; }
	while (n > 0) { buf[i++] = '0' + (n % 10); n /=  10; }
	for (int j = 0; j < i / 2; j++) {
		char tmp = buf[j];
		buf[j] = buf[i-1-j];
		buf[i-1-j] = tmp;
	}
	buf[i] = '\0';
	vga_print(buf, 7);
}

void heap_print_stats(void) {
	vga_print("Heap: ", 7);
	print_dec(total_allocated);
	vga_print(" B used / ", 7);
	print_dec(total_free);
	vga_print(" B free\n", 7);
}
