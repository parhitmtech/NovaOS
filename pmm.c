/* pmm.c - Physical Memory Manager: bitmap page allocator */
#include "pmm.h"

/* ── Kernel boundaries (defined by linker script) ─────────── */
/* The linker exports these symbols so we know exactly where our
 * kernel image starts and ends in physical memory. We must mark
 * these pages as used so the PMM never hands them out. */
extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];

/* ── Bitmap storage ───────────────────────────────────────── */
/* We need 1 bit per 4KB page. For 4GB of RAM: 4GB / 4KB = 1M pages = 128KB bitmap.
 * We reserve spacce for up to 4GB (1M pages = 32768 uint32_t entries).
 * This static array lives in .bss and is zeroed at startup. */
#define MAX_PAGES (1024 * 1024)   /* covers 4GB of physical RAM */
#define BITMAP_SIZE (MAX_PAGES / 32) /* 32 bits per uint32_t */

static uint32_t bitmap[BITMAP_SIZE];
static uint64_t total_pages = 0;
static uint64_t free_pages = 0;

/* ── Bitmap helpers ───────────────────────────────────────── */
static void bitmap_set(uint64_t page) {
	bitmap[page / 32] |= (1u << (page % 32));
}

static void bitmap_clear(uint64_t page) {
	bitmap[page / 32] &= ~(1u << (page % 32));
}

static int bitmap_test(uint64_t page) {
	return (bitmap[page / 32] >> (page % 32)) & 1;
}

/* ── Internal helpers ─────────────────────────────────────── */
static void pmm_mark_used(uint64_t base, uint64_t length) {
	uint64_t start_page = base / PMM_PAGE_SIZE;
	uint64_t end_page = (base + length + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
	for (uint64_t p = start_page; p < end_page && p < MAX_PAGES; p++) {
		if (!bitmap_test(p)) {
			bitmap_set(p);
			if (free_pages > 0) free_pages--;
		}
	}
}

static void pmm_mark_free(uint64_t base, uint64_t length) {
	uint64_t start_page = base / PMM_PAGE_SIZE;
	uint64_t end_page = (base + length + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
	for (uint64_t p = start_page; p < end_page && p < MAX_PAGES; p++) {
		if (bitmap_test(p)) {
			bitmap_clear(p);
			free_pages++;
		}
	}
}

/* ── Public API ───────────────────────────────────────────── */
void pmm_init(mb2_info_t* mb2_info) {
	/* Step 1: mark evrything used (safe default - never give out memory we haven't explicitly confirmed is usable). */
	for (uint64_t i = 0;i < BITMAP_SIZE;i++) {
		bitmap[i] = 0xFFFFFFFF;
	}
	free_pages = 0;

	/* Step 2: find the memory map tag from GRUB */
	mb2_mmap_tag_t* mmap_tag = (mb2_mmap_tag_t*) mb2_find_tag(mb2_info, MB2_TAG_MMAP);
	if (!mmap_tag) {
		/* No memory map - can't safety allocate anything.
		 * This shouldn't happen with GRUB, but be explicit. */
		return;
	}

	/* Step 3: walk every memory region GRUB reported */
	mb2_mmap_entry_t* entry = (mb2_mmap_entry_t*) ((uint8_t*) mmap_tag + 16);
	mb2_mmap_entry_t* end = (mb2_mmap_entry_t*) ((uint8_t*) mmap_tag + mmap_tag->size);

	while (entry < end) {
		if (entry->type == MB2_MMAP_AVAILABLE) total_pages += entry->length / PMM_PAGE_SIZE;

		if (entry->type == MB2_MMAP_AVAILABLE) {
			pmm_mark_free(entry->base_addr, entry->length);
		}

		entry = (mb2_mmap_entry_t*) ((uint8_t*) entry + mmap_tag->entry_size);
	}

	/* Step 4: re-mark the first 1MB as used (BIOS, VGA BUFFER at 0xB8000,
	 * legacy real-mode stuff - never safe to allocate from here). */
	pmm_mark_used(0, 0x100000);

	/* Step 5: re-mark our kernel image as used.
	 * The linker script exports _kernel_start and _kernel_end as physical
	 * addresses of the start and end of our loaded kernel binary. */

	uint64_t kstart = (uint64_t)(uint8_t*) _kernel_start;
	uint64_t kend = (uint64_t)(uint8_t*) _kernel_end;
	pmm_mark_used(kstart, kend - kstart);

	/* Step 6: make the PMM's own bitmap as used (it's in .bss, which is already inside the kernel image range - this is a 
	           double-mark, harmless, but worth noting explicitly for clarity). */
}

uint64_t pmm_alloc_page(void) {
	/* Linear scan - O(n). Acceptable for now, revisit with buddy allocator
	 * once we have a scheduler and care about allocation latency. */
	for (uint64_t i = 0; i < MAX_PAGES; i++) {
		if (!bitmap_test(i)) {
			bitmap_set(i);
			free_pages--;
			uint64_t addr = i * PMM_PAGE_SIZE;
			/* Zero the page before handling it out - critical for page tables,
			 * and good hygiene generally (no data leaks between allocations). */
			uint8_t* p = (uint8_t*) addr;
			for (uint64_t b = 0; b < PMM_PAGE_SIZE; b++) {
				p[b] = 0;
			}
			return addr;
		}
	}
	return 0; /* out of memory */
}

void pmm_free_page(uint64_t addr) {
	uint64_t page = addr / PMM_PAGE_SIZE;
	if (page >= MAX_PAGES) return;
	bitmap_clear(page);
	free_pages++;
}

/* Forward declare vga_print/vga_print_hex - same pattern as keyboard.c.
 * Will be cleaned up when VGA moves to its own header */
extern void vga_print(const char* str, int fg);
/* extern void vga_print_hex(uint64_t value, int fg); */

static void print_decimal(uint64_t n) {
	char buf[20];
	int i = 0;
	if (n == 0) { vga_print("0", 7); return; }
	while (n > 0) {
		buf[i++] = '0' + (n % 10);
		n /= 10;
	}
	/* reverse */
	for (int j = 0;j < i / 2;j++) {
		char tmp = buf[j];
		buf[j] = buf[i - 1 - j];
		buf[i - 1 - j] = tmp;
	}
	buf[i] = '\0';
	vga_print(buf, 7);
}

void pmm_print_stats(void) {
	vga_print("PMM:  ", 7);
	print_decimal(free_pages * PMM_PAGE_SIZE / 1024 / 1024);
	vga_print(" MB free / ", 7);
	print_decimal(total_pages * PMM_PAGE_SIZE / 1024 / 1024);
	vga_print(" MB total\n", 7);
}
