#if !defined(MEM_H)
#define MEM_H

#include "../types.h"

typedef struct {
    uint8_t flags;      // Allocation status flag (1 for allocated, 0 for free)
    uint32_t blockSize;  // Size of the block, including the header
}__attribute__((packed)) BlockHeader;

#define MALLOC_BEGIN_ADDR (uint8_t*)0x10000
#define MALLOC_BLOCK_HEADER_LENGTH (sizeof(BlockHeader))

/* copy `nbytes` bytes to `*source`, starting at `*dest` */
void memoryCopy(char *source, char *dest, int nbytes);

/* set the value of `nbytes` bytes to `val`, starting at `*dest` */
void memorySet(char *dest, char val, int nbytes);

void* malloc(uint32_t nbytes);

void dealloc(void *block);

#endif // MEM_H