#ifndef IPSOS_SYS_MMAN_H
#define IPSOS_SYS_MMAN_H
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_PRIVATE 2
#define MAP_SHARED 1
#define MAP_FIXED 16
#define MAP_FAILED ((void *)-1)
int mprotect(void *address, unsigned long length, int protection);
#endif
