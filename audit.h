#ifndef AUDIT_H__
#define AUDIT_H__

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

/* Syscalls */
long syscall0(int sysnr);
long syscall1(int sysnr, unsigned long arg0);
long syscall2(int sysnr, unsigned long arg0, unsigned long arg1);
long syscall3(int sysnr, unsigned long arg0, unsigned long arg1,
	      unsigned long arg2);
long syscall4(int sysnr, unsigned long arg0, unsigned long arg1,
	      unsigned long arg2, unsigned long arg3);
long syscall5(int sysnr, unsigned long arg0, unsigned long arg1,
	      unsigned long arg2, unsigned long arg3,
	      unsigned long arg4);

/* String functions */
size_t strlen(const char *x);
int strcmp(const char *a, const char *b);
void *memcpy(void *dest, const void *src, size_t size);

/* File functions */
int open(const char *path, long mode); /* Don't support O_CREAT */
void *mmap(void *addr, size_t length, int prot, int flags, int fd,
	   off_t offset);
int munmap(const void *base, size_t size);
ssize_t write(int fd, const void *buf, size_t size);
ssize_t read(int fd, void *buf, size_t size);
off_t lseek(int fd, off_t offset, int whence);

/* Other low-level bits */
void _exit(int code);

/* Other utilities. */
void putstr(const char *msg);
void put_n_strs(unsigned n, const char *str);
void putint(unsigned long x);

/* Stuff which the audit client has to export. */

/* Return 1 if you want to look at this function, and 0 otherwise. */
int audit_this_function(const char *name);

/* Called before the client calls a particular function, if
   audit_this_function() returned 1 for the function.  @args is the
   arguments to the function (we can't tell how many there are).
   Return 0 to run the function, or 1 to block it.  If 1 is returned
   by pre_func_audit, the library call returns *res (which can be up
   to 128 bits; we don't support modifying return values larger than
   that).  If 1 is returned by pre_func_audit, post_func_audit isn't
   called. */
int pre_func_audit(const char *name, unsigned long *args, unsigned long *res);

/* Called after a library call for which pre_func_audit() returned 0,
   provided the library call returns (so it won't get called if you
   e.g. longjmp out).  rv points at the return value, which might be
   up to 128 bits, and can be used to modify it. */
void post_func_audit(const char *name, unsigned long *rv);

#endif /* !AUDIT_H__ */
