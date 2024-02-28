#include "mem.h"
#include "../debug.h"

void memcpy(char *source, char *dest, int nbytes) {
    int i;
    for (i = 0; i < nbytes; i++) {
        *(dest + i) = *(source + i);
    }
}

void memset(char *dest, char val, int nbytes) {
    int i;
    for (i = 0; i < nbytes; i++) {
        *(dest + i) = val;
    }
}

bool memequal(char *a, char *b, int nbytes) {
    for (int i = 0; i < nbytes; i++) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

static void *find(uint32_t nbytes);

static void optimize();

static void split(void *block, uint32_t nbytes);

static void *biggestBlock = NULL;
static void *lastBlock = NULL;

void *malloc(uint32_t nbytes) {
    // LOG("Requested allocation of "); LOG_INT(nbytes); LOG(" bytes\n");

    void *block = MALLOC_BEGIN_ADDR;
    uint8_t *flags;
    uint32_t *blockSize;

    void *nextBlock;
    uint8_t *nextFlags;
    uint32_t *nextBlockSize;

    while (true) {
        flags = (uint8_t *)block;
        blockSize = (uint32_t *)(block+1);
        
        // LOG("  Checking block at "); // LOG is part of my header `debug.h`
        // LOG_INT(block);
        // LOG(": ");
        // LOG("flags=");
        // LOG_BYTE(*flags);
        // LOG(", bytes=");
        // LOG_INT(*blockSize - MALLOC_BLOCK_HEADER_LENGTH);
        // LOG("...");
        if ((*flags & 1) != 0)
        {
            // LOG(" Not a free block\n");
            block += *blockSize;
            continue;
        }
        // LOG(" Is a free block\n  ");

        // Melt as many following free blocks as possible into this one
        nextBlock = block + *blockSize;
        nextFlags = (uint8_t *)nextBlock;
        nextBlockSize = (uint32_t *)(nextBlock+1);
        if (*nextBlockSize <= MALLOC_BLOCK_HEADER_LENGTH) { // last block
            // LOG("Is last block, extending...\n");
            *(uint8_t *)block = 1;
            *(uint32_t *)(block+1) = nbytes + MALLOC_BLOCK_HEADER_LENGTH;
            return block + MALLOC_BLOCK_HEADER_LENGTH;
        }
        uint32_t melted = 0;
        while ((*nextFlags & 1) == 0 && *nextBlockSize > MALLOC_BLOCK_HEADER_LENGTH)
        {
            *blockSize += *nextBlockSize;

            nextBlock += *nextBlockSize;
            nextFlags = (uint8_t *)nextBlock;
            nextBlockSize = (uint32_t *)(nextBlock+1);
            melted++;
        }
        // LOG_INT(melted); LOG(" melted, ");
        if (*blockSize - MALLOC_BLOCK_HEADER_LENGTH < nbytes && *blockSize > MALLOC_BLOCK_HEADER_LENGTH)
        {
            // LOG("Too small\n");
            block += *blockSize;
            continue;
        }

        // LOG("Big enough to allocate, splitting block\n");

        if (*blockSize > nbytes+MALLOC_BLOCK_HEADER_LENGTH) {
            // Calculate the size of the remaining free block
            uint32_t remainingBlockSize = *blockSize - (nbytes + MALLOC_BLOCK_HEADER_LENGTH);

            // Update the size of the allocated block
            *blockSize = nbytes + MALLOC_BLOCK_HEADER_LENGTH;

            // Update the next block's header to reflect the remaining free block
            uint8_t *nextBlock = block + nbytes + MALLOC_BLOCK_HEADER_LENGTH;
            *(uint8_t *)nextBlock = 0; // Set the flag for a free block
            *(uint32_t *)(nextBlock + 1) = remainingBlockSize;

            // LOG("Block split, allocated "); LOG_INT(nbytes); LOG(" bytes, saved "); LOG_INT(remainingBlockSize); LOG("\n");
        } else {
            // LOG("Not enough excess to split the block\n");
        }

        *(uint8_t *)block = 1;
        *(uint32_t *)(block+1) = nbytes + MALLOC_BLOCK_HEADER_LENGTH;
        return block + MALLOC_BLOCK_HEADER_LENGTH;
    }
    // LOG("\n");
}

void free(void *ptr) {
    void *block = ptr - MALLOC_BLOCK_HEADER_LENGTH;
    // LOG("Requested deallocation\n");
    uint8_t *flags = (uint8_t *)block;
    uint32_t *blockSize = (uint32_t *)(block+1);
    // LOG("  ");
    // LOG("addr=");
    // LOG_INT(block);
    // LOG(", flags=");
    // LOG_BYTE(*flags);
    // LOG(", bytes=");
    // LOG_INT(*blockSize - MALLOC_BLOCK_HEADER_LENGTH);
    // LOG("\n");

    *flags &= ~1;

    void *nextBlock = block + *blockSize;
    uint8_t *nextFlags = (uint8_t *)nextBlock;
    uint32_t *nextBlockSize = (uint32_t *)(nextBlock+1);
    if (*nextBlockSize <= MALLOC_BLOCK_HEADER_LENGTH) {
        *blockSize = 0;
    }
}

void printMemoryInfo() {
    void *currentBlock = MALLOC_BEGIN_ADDR;
    BlockHeader *header;

    LOG("Memory Blocks Information:\n");
    uint32_t i = 0;
    bool allocated;
    bool reserved;
    while (true) {
        header = (BlockHeader *)currentBlock;

        // Check for the end of memory
        if (header->blockSize <= MALLOC_BLOCK_HEADER_LENGTH) {
            break;
        }

        LOG("  BLK "); LOG_INT(++i); LOG(" <"); LOG_INT(currentBlock); LOG("> ");
        LOG_INT(header->blockSize); LOG("(");
        LOG_INT(header->blockSize - MALLOC_BLOCK_HEADER_LENGTH); LOG(") bytes      ");

        allocated = ((header->flags) & 1);
        reserved = ((header->flags) & 2) > 0;

        if (reserved) {
            LOG("<Reserved>");
        } else if (allocated) {
            LOG("<Allocated>");
        } else {
            LOG("<Free>");
        }
        
        LOG("\n");
        // Move to the next block
        currentBlock += header->blockSize;
    }
}