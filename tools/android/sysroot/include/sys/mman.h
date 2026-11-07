#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H
#include <stddef.h>
#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_PRIVATE 0x02
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20
#define MAP_FIXED_NOREPLACE 0x100000
#define MAP_FAILED ((void *)-1)
void *mmap(void *addr, size_t size, int prot, int flags, int fd, long off);
int munmap(void *addr, size_t size);
#endif
