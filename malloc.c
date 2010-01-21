#include <stddef.h>
#include "audit.h"

#define ARENA_PAGES 4

#define MAX_ARENA_ALLOC_SIZE 1008

struct malloc_chunk {
	unsigned long arena_and_free;
};

struct malloc_arena {
	struct malloc_arena *prev;
	struct malloc_arena *next;
	unsigned chunk_size;
	unsigned free_chunks;
};

static struct audit_lock
malloc_lock;
static struct malloc_arena *
head_avail_arena;

static struct malloc_arena *
chunk_arena(struct malloc_chunk *c)
{
	return (struct malloc_arena *)(c->arena_and_free & ~(3ul));
}

static unsigned
chunk_free(struct malloc_chunk *c)
{
	return c->arena_and_free & 1;
}

static unsigned
chunks_per_arena(size_t chunk_size)
{
	return (ARENA_PAGES * PAGE_SIZE - sizeof(struct malloc_arena)) / (chunk_size + sizeof(struct malloc_chunk));
}

static void *
obtain_memory(unsigned nr_pages)
{
	return mmap(NULL, nr_pages * PAGE_SIZE,
		    PROT_READ|PROT_WRITE|PROT_EXEC,
		    MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
}

static struct malloc_chunk *
get_chunk(struct malloc_arena *ma, unsigned idx)
{
	return (struct malloc_chunk *)((void *)(ma + 1) + idx * (ma->chunk_size + sizeof(struct malloc_chunk)));
}

static struct malloc_arena *
new_malloc_arena(size_t size)
{
	struct malloc_arena *work;
	unsigned x;

	work = obtain_memory(ARENA_PAGES);
	work->chunk_size = size;
	work->prev = NULL;
	work->next = head_avail_arena;
	work->free_chunks = chunks_per_arena(size);
	for (x = 0; x < work->free_chunks; x++)
		get_chunk(work, x)->arena_and_free = (unsigned long)work | 3;
	head_avail_arena = work;
	return work;
}

static void *
malloc_arena(struct malloc_arena *arena)
{
	unsigned cursor;
	unsigned nr_chunks;
	struct malloc_chunk *c;

	nr_chunks = chunks_per_arena(arena->chunk_size);
	cursor = nr_chunks - arena->free_chunks;
	while (1) {
		c = get_chunk(arena, cursor);
		if (chunk_free(c)) {
			arena->free_chunks--;
			c->arena_and_free = (unsigned long)arena | 2;
			if (arena->free_chunks == 0) {
				if (arena->prev)
					arena->prev->next = arena->next;
				else
					head_avail_arena = arena->next;
				if (arena->next)
					arena->next->prev = arena->prev;
				arena->next = arena->prev = (struct malloc_arena *)0xf001;
			}
			return c + 1;
		}
		cursor++;
		if (cursor == nr_chunks)
			cursor = 0;
	}
}

void *
malloc(size_t size)
{
	void *res;
	struct malloc_arena *arena;

	size = (size + 15) & ~15;

	if (size > MAX_ARENA_ALLOC_SIZE) {
		size += sizeof(size);
		size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
		res = obtain_memory( (size + PAGE_SIZE - 1) / PAGE_SIZE);
		*(size_t *)res = size;
		return res + sizeof(size_t);
	}

	acquire_lock(&malloc_lock);
	for (arena = head_avail_arena;
	     arena != NULL && arena->chunk_size != size;
	     arena = arena->next)
		;
	if (!arena)
		arena = new_malloc_arena(size);
	res = malloc_arena(arena);
	release_lock(&malloc_lock);
	return res;
}

void
free(void *ptr)
{
	size_t pre;

	pre = ((size_t *)ptr)[-1];
	if (pre & 2) {
		struct malloc_chunk *mc;
		struct malloc_arena *ma;

		/* It's a chunk-based allocation */
		mc = ptr - sizeof(mc);
		ma = chunk_arena(mc);
		acquire_lock(&malloc_lock);
		if (ma->free_chunks == 0) {
			ma->prev = NULL;
			ma->next = head_avail_arena;
			if (head_avail_arena)
				head_avail_arena->prev = ma;
			head_avail_arena = ma;
		}
		ma->free_chunks++;
		mc->arena_and_free = (unsigned long)ma | 3;
		release_lock(&malloc_lock);
	} else {
		/* It's an mmap allocation */
		munmap(ptr - sizeof(size_t), pre);
	}
}
