#define _GNU_SOURCE
#include <link.h>
#include <stddef.h>
#include "audit.h"

unsigned
la_version(unsigned version)
{
	if (version == LAV_CURRENT)
		return LAV_CURRENT;
	else
		return 0;
}

unsigned
la_objopen(struct link_map *map, Lmid_t lmid, uintptr_t *cookie)
{
	return LA_FLG_BINDTO|LA_FLG_BINDFROM;
}

#define MAPALLOC_REGION_SIZE (4*PAGE_SIZE)
static void *
mapalloc_current_region;
static unsigned
mapalloc_current_region_bytes_left;

static void *
mapalloc(unsigned size)
{
	void *res;

	size = (size + 15) & ~15;

	if (mapalloc_current_region_bytes_left < size) {
		mapalloc_current_region = mmap(NULL, MAPALLOC_REGION_SIZE,
					       PROT_READ|PROT_WRITE|PROT_EXEC,
					       MAP_PRIVATE|MAP_ANONYMOUS,
					       -1, 0);
		mapalloc_current_region_bytes_left = MAPALLOC_REGION_SIZE;
	}
	res = mapalloc_current_region;
	mapalloc_current_region += size;
	return res;
}

extern unsigned char trampoline_1_start, trampoline_1_end,
	trampoline_1_load_target, trampoline_1_load_name,
	trampoline_1_load_find_second_stage;

extern unsigned char find_second_stage;

#define TRAMPOLINE_1_OFFSET(ofs) ((unsigned long)(&trampoline_1_ ## ofs - &trampoline_1_start))
#define TRAMPOLINE_1_SIZE TRAMPOLINE_1_OFFSET(end)

/* Allocate a phase 1 trampoline */
static void *
allocate_trampoline(const char *symname, const void *addr)
{
	void *buf;
	const unsigned symname_size = strlen(symname) + 1;

	buf = mapalloc(TRAMPOLINE_1_SIZE + symname_size);
	memcpy(buf, &trampoline_1_start, TRAMPOLINE_1_SIZE);
	memcpy(buf + TRAMPOLINE_1_SIZE, symname, symname_size);
	*(unsigned long *)(buf + TRAMPOLINE_1_OFFSET(load_target) + 2) =
		(unsigned long)addr;
	*(unsigned long *)(buf + TRAMPOLINE_1_OFFSET(load_name) + 2) =
		(unsigned long)buf + TRAMPOLINE_1_SIZE;
	*(unsigned long *)(buf + TRAMPOLINE_1_OFFSET(load_find_second_stage) + 2) =
		(unsigned long)&find_second_stage;

	return buf;
}

uintptr_t
la_symbind64(Elf64_Sym *sym, unsigned idx, uintptr_t *refcook,
	     uintptr_t *defcook, unsigned *flags, const char *symname)
{
	/* Don't try to interpose on non-function relocations */
	if (ELF64_ST_TYPE(sym->st_info) != STT_FUNC)
		return sym->st_value;

	return (uintptr_t)allocate_trampoline(symname, (void *)sym->st_value);
}

struct second_stage_trampoline {
	struct second_stage_trampoline *next;
	unsigned long target;
	unsigned long retaddr;
	unsigned char body[];
};

#define NR_HASH_BUCKETS 4096

static unsigned
sst_hash(unsigned long target, unsigned long ra)
{
	target ^= ra;
	target ^= target / NR_HASH_BUCKETS;
	return target % NR_HASH_BUCKETS;
}

static struct second_stage_trampoline *
sst_hash_tab[NR_HASH_BUCKETS];

extern unsigned char trampoline_2_start,
	trampoline_2_loads_target_name,
	trampoline_2_loads_call_post_func,
	trampoline_2_loads_return_rip,
	trampoline_2_end;

extern unsigned char call_post_func;

#define TRAMPOLINE_2_OFFSET(ofs) ((unsigned long)(&trampoline_2_ ## ofs - &trampoline_2_start))
#define TRAMPOLINE_2_SIZE TRAMPOLINE_2_OFFSET(end)

static struct second_stage_trampoline *
build_sst(unsigned long target_rip, unsigned long return_rip, const char *name)
{
	struct second_stage_trampoline *sst;
	void *buf;
	unsigned name_size = strlen(name) + 1;

	sst = mapalloc(name_size + TRAMPOLINE_2_SIZE + sizeof(*sst));
	buf = sst->body;
	memcpy(buf, &trampoline_2_start, TRAMPOLINE_2_SIZE);
	*(unsigned long *)(buf + TRAMPOLINE_2_OFFSET(loads_target_name) + 2) =
		(unsigned long)name;
	*(unsigned long *)(buf + TRAMPOLINE_2_OFFSET(loads_call_post_func) + 2) =
		(unsigned long)&call_post_func;
	*(unsigned long *)(buf + TRAMPOLINE_2_OFFSET(loads_return_rip) + 2) =
		(unsigned long)return_rip;

	sst->target = target_rip;
	sst->retaddr = return_rip;
	return sst;
}

void *
find_second_stage_trampoline(unsigned long target, unsigned long ra,
			     const char *name)
{
	struct second_stage_trampoline **pprev, *cursor, **head;

	head = pprev = &sst_hash_tab[sst_hash(target, ra)];
	cursor = *pprev;
	while (cursor) {
		if (cursor->target == target &&
		    cursor->retaddr == ra)
			break;
		pprev = &cursor->next;
		cursor = *pprev;
	}

	if (!cursor)
		cursor = build_sst(target, ra, name);
	*pprev = cursor->next;
	cursor->next = *head;
	*head = cursor;
	return cursor->body;
}
