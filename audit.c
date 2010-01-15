#define _GNU_SOURCE
#include <asm/unistd.h>
#include <link.h>
#include <stddef.h>

#define PAGE_SIZE 4096

#define SEEK_SET 0
#define SEEK_END 2

#define O_RDONLY 0

#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4

#define MAP_PRIVATE 2
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20

static size_t
strlen(const char *x)
{
	int res;
	for (res = 0; x[res]; res++)
		;
	return res;
}

static int
strcmp(const char *a, const char *b)
{
	while (*a == *b && *a) {
		a++;
		b++;
	}
	return *a - *b;
}

static void *
memcpy(void *dest, const void *src, size_t size)
{
	unsigned x;
	for (x = 0; x < size; x++)
		((unsigned char *)dest)[x] = ((unsigned char *)src)[x];
	return dest;
}

static inline long
syscall(int sysnr, unsigned long arg0, unsigned long arg1, unsigned long arg2,
	unsigned long arg3, unsigned long arg4, unsigned long arg5)
{
	long res;
	long rcx;
	register long r11 asm ("r11");
	register long r10 asm ("r10") = arg3;
	register long r8 asm ("r8") = arg4;
	register long r9 asm ("r9") = arg5;

	asm ("syscall"
	     : "=a" (res), "=c" (rcx), "=r" (r11)
	     : "0" (sysnr), "D" (arg0), "S" (arg1), "d" (arg2), "r" (r10),
	       "r" (r8), "r" (r9)
	     : "memory", "cc");
	return res;
}

static long
syscall0(int sysnr)
{
	return syscall(sysnr, 0, 0, 0, 0, 0, 0);
}

static long
syscall1(int sysnr, unsigned long arg0)
{
	return syscall(sysnr, arg0, 0, 0, 0, 0, 0);
}

static long
syscall2(int sysnr, unsigned long arg0, unsigned long arg1)
{
	return syscall(sysnr, arg0, arg1, 0, 0, 0, 0);
}

static long
syscall3(int sysnr, unsigned long arg0, unsigned long arg1,
	 unsigned long arg2)
{
	return syscall(sysnr, arg0, arg1, arg2, 0, 0, 0);
}

static long
syscall4(int sysnr, unsigned long arg0, unsigned long arg1,
	 unsigned long arg2, unsigned long arg3)
{
	return syscall(sysnr, arg0, arg1, arg2, arg3, 0, 0);
}

static long
syscall5(int sysnr, unsigned long arg0, unsigned long arg1,
	 unsigned long arg2, unsigned long arg3,
	 unsigned long arg4)
{
	return syscall(sysnr, arg0, arg1, arg2, arg3, arg4, 0);
}

static void
_exit(int code)
{
	syscall1(__NR_exit, code);
}

/* Don't support O_CREAT */
static int
open(const char *path, long mode)
{
	return syscall2(__NR_open, (unsigned long)path, mode);
}

static ssize_t
write(int fd, const void *buf, size_t size)
{
	return syscall3(__NR_write, fd, (unsigned long)buf, size);
}

static ssize_t
read(int fd, void *buf, size_t size)
{
	return syscall3(__NR_read, fd, (unsigned long)buf, size);
}

static off_t
lseek(int fd, off_t offset, int whence)
{
	return syscall3(__NR_lseek, fd, offset, whence);
}

static void *
mmap(void *addr, size_t length, int prot, int flags, int fd,
     off_t offset)
{
	return (void *)syscall(__NR_mmap, (unsigned long)addr, length,
			       prot, flags, fd, offset);
}

static int
munmap(const void *base, size_t size)
{
	return syscall2(__NR_munmap, (unsigned long)base, size);
}

static void
putstr(const char *msg)
{
	write(1, msg, strlen(msg));
}

static void
put_n_strs(unsigned n, const char *str)
{
	while (n--)
		putstr(str);
}

static void
putint(unsigned long x)
{
	char m[32];
	char *ptr = m + 31;
	if (x == 0) {
		putstr("0");
		return;
	}
	m[31] = 0;
	while (x) {
		ptr--;
		if (x % 16 < 10) {
			*ptr = (x % 16) + '0';
		} else {
			*ptr = (x % 16) + 'a' - 10;
		}
		x /= 16;
	}
	putstr(ptr);
}

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

static int call_depth;

int
pre_func_audit(const char *name, unsigned long *args, unsigned long *res)
{
	put_n_strs(call_depth, "    ");
	putstr(name);
	putstr("(");
	if (!strcmp(name, "__asprintf_chk")) {
		putint(args[0]);
		putstr(", ");
		putint(args[1]);
		putstr(", ");
		putstr(args[2]);
	} else if (!strcmp(name, "realloc")) {
		putint(args[0]);
		putstr(", ");
		putint(args[1]);
	} else if (!strcmp(name, "fopen64")) {
		putstr((const char *)args[0]);
		putstr(", ");
		putstr((const char *)args[1]);
	} else if (!strcmp(name, "malloc") ||
		   !strcmp(name, "free")) {
		putint(args[0]);
	} else if (!strcmp(name, "getenv")) {
		putstr(args[0]);
	}
	putstr(");\n");
	call_depth++;
	return 0;
}

void
post_func_audit(const char *name, unsigned long *rv)
{
	put_n_strs(call_depth, "    ");
	putstr(" -> ");
	if (!strcmp(name, "free"))
		putstr("<void>");
	else
		putint(rv[0]);
	putstr("\n");
	call_depth--;
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
