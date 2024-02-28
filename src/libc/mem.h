#if !defined(MEM_H)
#define MEM_H

#include "../types.h"

typedef struct {
    uint8_t flags;      // Allocation status flags
    // bit 0: occupied
    // bit 1: reserved
    uint32_t blockSize;  // Size of the block, including the header
}__attribute__((packed)) BlockHeader;

#define MALLOC_BEGIN_ADDR (void*)0x00100000
#define MALLOC_BLOCK_HEADER_LENGTH (sizeof(BlockHeader))

/* copy `nbytes` bytes to `*source`, starting at `*dest` */
void memcpy(char *source, char *dest, int nbytes);

/* set the value of `nbytes` bytes to `val`, starting at `*dest` */
void memset(char *dest, char val, int nbytes);

bool memequal(char *a, char *b, int nbytes);

void* malloc(uint32_t nbytes);

void free(void *block);

#endif // MEM_H