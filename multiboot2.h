#pragma once
/*
 * multiboot2.h - MultiBoot2 info structure definitions
 * GRUB passes a pointer to this structure in ebx/rbx before jumping to our kernel. We walk the tag list to find the
   memory map, which tells us what physical RAM actually exists and what's safe to use.
 * Spec reference: https://www.gnu.org/software/grub/manual/multiboot2/
 */
#include <stdint.h>

/* ── Top-level header ─────────────────────────────────────── */
typedef struct {
	uint32_t total_size; /* total size of the info structure in bytes */
	uint32_t reserved;
	/* tags follow immediately after, 8-byte aligned */
} __attribute__ ((packed)) mb2_info_t;

/* ── Generic tag header (every tag starts with these 8 bytes) ── */
typedef struct {
	uint32_t type;
	uint32_t size;
} __attribute__((packed)) mb2_tag_t;

/* ── Tag types we care about ─────────────────────────────── */
#define MB2_TAG_END	0
#define MB2_TAG_MMAP	6	/* memory map */

/* ── Memory map tag (type 6) ─────────────────────────────── */
typedef struct {
	uint32_t type;
	uint32_t size;
	uint32_t entry_size;  /* size of each mb2_mmap_entry_t, usually 24 */
	uint32_t entry_version; /* currently 0 */
	/* array of mb2_mmap_entry_t follows */
} __attribute__((packed)) mb2_mmap_tag_t;

/* ── One memory region entry ──────────────────────────────── */
typedef struct {
	uint64_t base_addr;
	uint64_t length;
	uint32_t type; /* 1 = usable RAM, anything else = don't touch */
	uint32_t reserved;
} __attribute__((packed)) mb2_mmap_entry_t;

/* Memory region types */
#define MB2_MMAP_AVAILABLE  1  /* free RAM we can use */
#define MB2_MMAP_RESERVED   2
#define MB2_MMAP_ACPI       3
#define MB2_MMAP_NVS        4
#define MB2_MMAP_BADRAM     5

/* ── Helper: walk tags ────────────────────────────────────── */
/* Returns a pointer to the first tag of the given type, or NULL.
 * Usage: mb2_mmap_tag_t* mmap = (mb2_mmap_tag_t*) mb2_find_tag(info, MB2_TAG_MMAP); */
static inline mb2_tag_t* mb2_find_tag(mb2_info_t* info, uint32_t type) {
	/* First tag starts immediately after the 8-byte header */
	mb2_tag_t* tag = (mb2_tag_t*) ((uint8_t*) info + 8);

	while (tag->type != MB2_TAG_END) {
		if (tag->type == type) {
			return tag;
		}
		/* Each tag is padded to 8-byte alignment */
		uint32_t size = (tag->size + 7) & ~7u;
		tag = (mb2_tag_t*) ((uint8_t*) tag + size);
	}
	return 0; /* not found */
}
