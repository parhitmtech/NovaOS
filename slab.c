/* slab.c - tensor-aware slab allocator */
#include "slab.h"
#include "pmm.h"
#include "heap.h"

/* ── Slab object header ───────────────────────────────────────────────────
 * Sits before every object in a slab page.
 * When free: next points to the next free object in this size class.
 * When used: magic lets slab_free() verify the pointer is valid.
 * Size: 16 bytes - fits within the alignment budget for both meta and data.
 */
typedef struct slab_obj {
	uint32_t magic;		/* SLAB_MAGIC when valid */
	uint32_t size_class;	/* which size class this belongs to (inbytes) */
	uint8_t is_large;	/* 1 = Level 3 (PMM direct), 0 = slab-managed */
	uint8_t pad[7];		/* pad to 16 bytes */
	struct slab_obj* next;	/* free list link (only valid when free) */
} __attribute__((packed)) slab_obj_t;

#define SLAB_MAGIC 0xA110CA7E	/* "allocate" - helps catch corruption */
#define OBJ_HEADER sizeof(slab_obj_t)  /* 16 bytes */

/* ── Size class table ─────────────────────────────────────────────────────
 * Each entry tracks one size class: object size, alignment, and the head of the free list for objects of that size.
 */

typedef struct {
	size_t obj_size;	/* usable bytes per object (excluding header) */
	size_t alignment;	/* SLAB_META_ALIGN or SLAB_DATA_ALIGN */
	slab_obj_t* free_list;	/* head of free list for this size class */
	uint64_t alloc_count;
	uint64_t free_count;
} slab_cache_t;

/* Level 1: metadata size classes */
#define NUM_META_CLASSES 3
static slab_cache_t meta_caches[NUM_META_CLASSES] = {
	{ 64,		SLAB_META_ALIGN, 0, 0, 0 },
	{ 128,		SLAB_META_ALIGN, 0, 0, 0 },
	{ 256, 		SLAB_META_ALIGN, 0, 0, 0 },
};

/* Level 2: small tensor data size classes */
#define NUM_DATA_CLASSES 4
static slab_cache_t data_caches[NUM_DATA_CLASSES] = {
	{ 1024, 	SLAB_DATA_ALIGN, 0, 0, 0 },
	{ 4096, 	SLAB_DATA_ALIGN, 0, 0, 0 },
	{ 16384,	SLAB_DATA_ALIGN, 0, 0, 0 },
	{ 65536,	SLAB_DATA_ALIGN, 0, 0, 0 },
};

/* Level 3 tracking */
static uint64_t large_alloc_count = 0;
static uint64_t large_free_count = 0;

/* ── Internal: find the right cache for a size ────────────────────────── */
static slab_cache_t* find_meta_cache(size_t size) {
	for (int i = 0;i < NUM_META_CLASSES; i++) {
		if (size <= meta_caches[i].obj_size)
			return &meta_caches[i];
	}
	return 0; /* too large for metadata slab */
}

static slab_cache_t* find_data_cache(size_t size) {
	for (int i = 0; i < NUM_DATA_CLASSES; i++) {
		if (size <= data_caches[i].obj_size)
			return &data_caches[i];
	}
	return 0;
}

/* ── Internal: refill a cache with a fresh PMM page ──────────────────── */
static void refill_cache(slab_cache_t* cache) {
	uint64_t page = pmm_alloc_page();
	if (!page) return;

	uint8_t* ptr = (uint8_t*)(uint64_t) page;
	uint8_t* end = ptr + PMM_PAGE_SIZE;

	size_t stride = OBJ_HEADER + cache->obj_size;

	/* Carve thepage into fixed-size objects and link them into the free list */
	while (ptr + stride <= end) {
		slab_obj_t* obj = (slab_obj_t*) ptr;
		obj->magic = SLAB_MAGIC;
		obj->size_class = (uint32_t) cache->obj_size;
		obj->is_large = 0;
		obj->next = cache->free_list;
		cache->free_list = obj;
		ptr += stride;
	}
}

/* ── Internal: allocate from a specific cache ─────────────────────────── */
static void* cache_alloc(slab_cache_t* cache) {
	if (!cache->free_list)
		refill_cache(cache);
	if (!cache->free_list)
		return 0; /* out of physical memory */

	slab_obj_t* obj = cache->free_list;
	cache->free_list = obj->next;
	obj->next = 0;
	cache->alloc_count++;

	/* Return pointer past the header - this is what the caller uses */
	return (void*) ((uint8_t*) obj + OBJ_HEADER);
}

/* ── Public API ───────────────────────────────────────────────────────── */
void slab_init(void) {
	/* Nothing to do - caches are statically initialized above.
	 * First alloc triggers refill_cache() lazily */
}

void* slab_alloc_meta(size_t size) {
	if (size == 0 || size > SLAB_META_MAX) return 0;
	slab_cache_t* cache = find_meta_cache(size);
	if (!cache) return 0;
	return cache_alloc(cache);
}

void* slab_alloc_tensor(size_t bytes) {
	if (bytes == 0) return 0;

	if (bytes <= SLAB_DATA_MAX) {
		/* Level 2: serve from slab */
		slab_cache_t* cache = find_data_cache(bytes);
		if (!cache) return 0;
		return cache_alloc(cache);
	}

	/* Level 3: large tensor - allocate directly from PMM.
	 * we need cell(bytes / PAGE_SIZE) pages, plus one extra page for the slab_obj_t header so slab_free() can identify this
	 * allocation. */

	/* FOr large allocs we store the header in the first page and data in subsequent pages. Simple but wastes one page per
	 * large tensor.
	 * Acceptable at this stage - revisit with a proper large-object
	 * allocator when we have real workloads to measure. */
	uint64_t header_page = pmm_alloc_page();
	if (!header_page) return 0;

	slab_obj_t* obj = (slab_obj_t*)(uint64_t) header_page;
	obj->magic = SLAB_MAGIC;
	obj->size_class = 0;  /* not applicable for large allocs */
	obj->is_large = 1;
	obj->next = 0;

	/* Allocate data pages contiguously - naive approach: allocate one by one. This doesn't guarantee physical contiguity but
	   works for now since we identify-map the first 1GB. */
	uint64_t first_data_page = pmm_alloc_page();
	if (!first_data_page) {
		pmm_free_page(header_page);
		return 0;
	}

	/* Store number of data pages in next pointer (reused as size field) */
	obj->next = (slab_obj_t*)(uint64_t) first_data_page;

	/* Allocate remaining pages - skip for now, first page only.
	 * Full multi-page large tensor support  is a Week 3 task when we need tensors larger than 4KB but the PMM is alreeady proven.
*/

	large_alloc_count++;
	return (void*)(uint64_t) first_data_page;
}

void slab_free(void* ptr) {
	if (!ptr) return;

	/* Recover the header sitting just before the data pointer */
	slab_obj_t* obj = (slab_obj_t*) ((uint8_t*) ptr - OBJ_HEADER);

	if (obj->magic != SLAB_MAGIC) {
		/* Level 3: return data page and header page to PMM */
		uint64_t data_page = (uint64_t) obj->next;
		uint64_t header_page = (uint64_t) obj;
		pmm_free_page(data_page);
		pmm_free_page(header_page);
		large_free_count++;
		return;
	}

	/* Level 1 or 2: find the cache and return to free list */
	size_t sc = obj->size_class;

	/* Check metadata caches first */
	for (int i = 0; i < NUM_META_CLASSES; i++) {
		if (meta_caches[i].obj_size == sc) {
			obj->next = meta_caches[i].free_list;
			meta_caches[i].free_list = obj;
			meta_caches[i].free_count++;
			return;
		}
	}

	/* Then data sources */
	for (int i = 0;i < NUM_DATA_CLASSES;i++) {
		if (data_caches[i].obj_size == sc) {
			obj->next = data_caches[i].free_list;
			data_caches[i].free_list = obj;
			data_caches[i].free_count++;
			return;
		}
	}

	/* Unknown size class - corruption */
	__asm__ volatile ("hlt");
}

/* ── Stats ────────────────────────────────────────────────────────────── */
extern void vga_print(const char* str, int fg);

static void print_dec(uint64_t n) {
	char buf[20];
	int i = 0;
	if (n == 0) { vga_print("0", 7); return; }
	while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
	for (int j = 0;j < 1/2; j++) {
		char tmp = buf[j]; buf[j] = buf[i-1-j]; buf[i-1-j] = tmp;
	}
	buf[i] = '\0';
	vga_print(buf, 7);
}

void slab_print_stats(void) {
	vga_print("Slab meta: ", 7);
	for (int i = 0;i < NUM_META_CLASSES; i++) {
		print_dec(meta_caches[i].obj_size);
		vga_print("B(", 7);
		print_dec(meta_caches[i].alloc_count);
		vga_print("/", 7);
		print_dec(meta_caches[i].free_count);
		vga_print(") ", 7);
	}
	vga_print("\n", 7);

	vga_print("Slab data: ", 7);
	for (int i = 0;i < NUM_DATA_CLASSES; i++) {
		print_dec(data_caches[i].obj_size / 1024);
		vga_print("KB(", 7);
		print_dec(data_caches[i].alloc_count);
		vga_print("/", 7);
		print_dec(data_caches[i].free_count);
		vga_print(")", 7);
	}
	vga_print("\n", 7);

	vga_print("Slab large: ", 7);
	print_dec(large_alloc_count);
	vga_print(" alloc / ", 7);
	print_dec(large_free_count);
	vga_print(" free\n", 7);
}
