#ifndef AUDIT_H__
#define AUDIT_H__

#include <sys/types.h>

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

#define FUTEX_WAIT_PRIVATE              128
#define FUTEX_WAKE_PRIVATE              129

/* Have to hide *everything* in order to stop RTLD from screwing
   up. */

#define HIDDEN __attribute__((visibility ("hidden")))

/* Syscalls */
long syscall0(int sysnr) HIDDEN;
long syscall1(int sysnr, unsigned long arg0) HIDDEN;
long syscall2(int sysnr, unsigned long arg0, unsigned long arg1) HIDDEN;
long syscall3(int sysnr, unsigned long arg0, unsigned long arg1,
	      unsigned long arg2) HIDDEN;
long syscall4(int sysnr, unsigned long arg0, unsigned long arg1,
	      unsigned long arg2, unsigned long arg3) HIDDEN;
long syscall5(int sysnr, unsigned long arg0, unsigned long arg1,
	      unsigned long arg2, unsigned long arg3,
	      unsigned long arg4) HIDDEN;

/* String functions */
size_t strlen(const char *x) HIDDEN;
int strcmp(const char *a, const char *b) HIDDEN;
void *memcpy(void *dest, const void *src, size_t size) HIDDEN;
void *memset(void *buf, int c, size_t n) HIDDEN;

/* File functions */
int open(const char *path, long mode) HIDDEN; /* Don't support O_CREAT */
void *mmap(void *addr, size_t length, int prot, int flags, int fd,
	   off_t offset) HIDDEN;
int munmap(const void *base, size_t size) HIDDEN;
ssize_t write(int fd, const void *buf, size_t size) HIDDEN;
ssize_t read(int fd, void *buf, size_t size) HIDDEN;
off_t lseek(int fd, off_t offset, int whence) HIDDEN;

/* Synchronisation */
/* locked is either 0 or 1, and waiters is an upper bound on the
   number of threads which are blocked waiting for the lock. */
struct audit_lock {
	int locked;
	int waiters;
};
void init_lock(struct audit_lock *al) HIDDEN; /* Or just memset to zero */
void acquire_lock(struct audit_lock *al) HIDDEN;
void release_lock(struct audit_lock *al) HIDDEN;

/* Other low-level bits */
void _exit(int code) HIDDEN;

/* Other utilities. */
void putstr(const char *msg) HIDDEN;
void put_n_strs(unsigned n, const char *str) HIDDEN;
void putint(unsigned long x) HIDDEN;

/* Malloc-like */
void * malloc(size_t size) HIDDEN;
void free(void *ptr) HIDDEN;

/* Stuff which the audit client has to export. */

/* Return 1 if you want to look at this function, and 0 otherwise. */
int audit_this_function(const char *name) HIDDEN;

/* Called before the client calls a particular function, if
   audit_this_function() returned 1 for the function.  @args is the
   arguments to the function (we can't tell how many there are).
   Return 0 to run the function, or 1 to block it.  If 1 is returned
   by pre_func_audit, the library call returns *res (which can be up
   to 128 bits; we don't support modifying return values larger than
   that).  If 1 is returned by pre_func_audit, post_func_audit isn't
   called.  val is a pointer to the actual function which is being
   called. */
int pre_func_audit(const char *name, unsigned long *args, unsigned long *res,
		   unsigned long val) HIDDEN;

/* Called after a library call for which pre_func_audit() returned 0,
   provided the library call returns (so it won't get called if you
   e.g. longjmp out).  rv points at the return value, which might be
   up to 128 bits, and can be used to modify it. */
void post_func_audit(const char *name, unsigned long *rv) HIDDEN;

#endif /* !AUDIT_H__ */
