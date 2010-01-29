/* Various very basic bits of functionality which aren't otherwise
   available in an audit library. */
#include <asm/unistd.h>
#include <link.h>
#include <stddef.h>
#include "audit.h"

size_t
strlen(const char *x)
{
	int res;
	for (res = 0; x[res]; res++)
		;
	return res;
}

int
strcmp(const char *a, const char *b)
{
	while (*a == *b && *a) {
		a++;
		b++;
	}
	return *a - *b;
}

void *
memcpy(void *dest, const void *src, size_t size)
{
	unsigned x;
	for (x = 0; x < size; x++)
		((unsigned char *)dest)[x] = ((unsigned char *)src)[x];
	return dest;
}

void *
memset(void *buf, int c, size_t n)
{
	unsigned x;
	for (x = 0; x < n; x++)
		((unsigned char *)buf)[x] = (unsigned char)c;
	return buf;
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

long
syscall0(int sysnr)
{
	return syscall(sysnr, 0, 0, 0, 0, 0, 0);
}

long
syscall1(int sysnr, unsigned long arg0)
{
	return syscall(sysnr, arg0, 0, 0, 0, 0, 0);
}

long
syscall2(int sysnr, unsigned long arg0, unsigned long arg1)
{
	return syscall(sysnr, arg0, arg1, 0, 0, 0, 0);
}

long
syscall3(int sysnr, unsigned long arg0, unsigned long arg1,
	 unsigned long arg2)
{
	return syscall(sysnr, arg0, arg1, arg2, 0, 0, 0);
}

long
syscall4(int sysnr, unsigned long arg0, unsigned long arg1,
	 unsigned long arg2, unsigned long arg3)
{
	return syscall(sysnr, arg0, arg1, arg2, arg3, 0, 0);
}

long
syscall5(int sysnr, unsigned long arg0, unsigned long arg1,
	 unsigned long arg2, unsigned long arg3,
	 unsigned long arg4)
{
	return syscall(sysnr, arg0, arg1, arg2, arg3, arg4, 0);
}

void
_exit(int code)
{
	syscall1(__NR_exit, code);
	/* Shouldn't get here */
	asm volatile ("int3\n");
	/* *really* shouldn't get here */
	while (1)
		;
}

int
open(const char *path, long mode)
{
	return syscall2(__NR_open, (unsigned long)path, mode);
}

ssize_t
write(int fd, const void *buf, size_t size)
{
	return syscall3(__NR_write, fd, (unsigned long)buf, size);
}

ssize_t
read(int fd, void *buf, size_t size)
{
	return syscall3(__NR_read, fd, (unsigned long)buf, size);
}

off_t
lseek(int fd, off_t offset, int whence)
{
	return syscall3(__NR_lseek, fd, offset, whence);
}

void *
mmap(void *addr, size_t length, int prot, int flags, int fd,
     off_t offset)
{
	return (void *)syscall(__NR_mmap, (unsigned long)addr, length,
			       prot, flags, fd, offset);
}

int
munmap(const void *base, size_t size)
{
	return syscall2(__NR_munmap, (unsigned long)base, size);
}

void
putstr(const char *msg)
{
	write(1, msg, strlen(msg));
}

void
put_n_strs(unsigned n, const char *str)
{
	while (n--)
		putstr(str);
}

void
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

void
putdecint(unsigned long x)
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
		*ptr = (x % 10) + '0';
		x /= 10;
	}
	putstr(ptr);
}


void
init_lock(struct audit_lock *al)
{
	memset(al, 0, sizeof(*al));
}

static inline int
cmpxchg(int *val, int from, int to)
{
	int res;
	asm volatile ("lock cmpxchg %2, %3"
		      : "=a" (res)
		      : "0" (from), "r" (to), "m" (*val)
		      : "memory", "cc");
	return res;
}

static inline void
atomic_inc(int *val)
{
	asm volatile ("lock incl %0"
		      :
		      : "m" (*val)
		      : "memory", "cc");
}

static inline void
atomic_dec(int *val)
{
	asm volatile ("lock decl %0"
		      :
		      : "m" (*val)
		      : "memory", "cc");
}

static inline void
rmb(void)
{
	asm volatile ("" ::: "memory");
}

void
acquire_lock(struct audit_lock *al)
{
	int old;
	int new;

top:
	old = al->locked;

	/* Try an uncontended acquire */
	while (old == 0) {
		new = cmpxchg(&al->locked, 0, 1);
		if (new == old)
			return;
		old = new;
	}

	/* Failed, add ourselves to the waitlist. */
	atomic_inc(&al->waiters);

	/* Go to sleep. */
	syscall4(__NR_futex, (unsigned long)&al->locked, FUTEX_WAIT_PRIVATE, 1, 0);

	/* Try it again. */
	atomic_dec(&al->waiters);
	goto top;
}

void
release_lock(struct audit_lock *al)
{
	al->locked = 0;
	rmb();
	if (al->waiters)
		syscall3(__NR_futex, (unsigned long)&al->locked, FUTEX_WAKE_PRIVATE, 1);
}
