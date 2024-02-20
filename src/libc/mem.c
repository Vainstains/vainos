#include "mem.h"
#include "../debug.h"

void memoryCopy(char *source, char *dest, int nbytes) {
    int i;
    for (i = 0; i < nbytes; i++) {
        *(dest + i) = *(source + i);
    }
}

void memorySet(char *dest, char val, int nbytes) {
    int i;
    for (i = 0; i < nbytes; i++) {
        *(dest + i) = val;
    }
}

static void *find(uint32_t nbytes);

static void optimize();

static void split(void *block, uint32_t nbytes);

static void *biggestBlock = NULL;
static void *lastBlock = NULL;

void *malloc(uint32_t nbytes) {
    LOG("Requested allocation of "); LOG_INT(nbytes); LOG(" bytes\n");

    optimize();

    void *block = find(nbytes);

    if (block == NULL) {
        LOG("Allocation failed\n");
        return NULL;
    }

    split(block, nbytes);

    return block;
}

void dealloc(void *ptr) {
    void *block = ptr - MALLOC_BLOCK_HEADER_LENGTH;
    LOG("Requested deallocation\n");
    uint8_t *flags = (uint8_t *)block;
    uint32_t *blockSize = (uint32_t *)(block+1);
    LOG("  ");
    LOG("addr=");
    LOG_INT(block);
    LOG(", flags=");
    LOG_BYTE(*flags);
    LOG(", bytes=");
    LOG_INT(*blockSize - MALLOC_BLOCK_HEADER_LENGTH);
    LOG("\n");

    *flags &= ~1;

    void *nextBlock = block + *blockSize;
    uint8_t *nextFlags = (uint8_t *)nextBlock;
    uint32_t *nextBlockSize = (uint32_t *)(nextBlock+1);
    if (*nextBlockSize <= MALLOC_BLOCK_HEADER_LENGTH) {
        *blockSize = 0;
    }
    optimize();
}

static void *find(uint32_t nbytes) {
    void *block = MALLOC_BEGIN_ADDR;
    BlockHeader *header = (BlockHeader *)block;

    while (true) {
        header = (BlockHeader *)block;
        if (header->blockSize <= MALLOC_BLOCK_HEADER_LENGTH)
            return block;
        if (header->flags != 0) {
            block += header->blockSize;
            continue;
        }
        // Check if the block is big enough to accommodate nbytes
        if (header->blockSize - MALLOC_BLOCK_HEADER_LENGTH >= nbytes && header->blockSize > MALLOC_BLOCK_HEADER_LENGTH) {
            return block;
        }
        // Move to the next block
        block += header->blockSize;
    }

    return NULL;
}

static void optimize() {
    void *block = MALLOC_BEGIN_ADDR;
    BlockHeader *header;
    void *largestBlock = NULL;
    uint32_t largestBlockSize = 0;

    while (true) {
        header = (BlockHeader *)block;

        if (header->flags == 0) {
            // Melt consecutive free blocks
            void *nextBlock = block + header->blockSize;
            BlockHeader *nextHeader = (BlockHeader *)nextBlock;

            while (nextHeader->flags == 0 && nextHeader->blockSize > MALLOC_BLOCK_HEADER_LENGTH) {
                header->blockSize += nextHeader->blockSize;

                nextBlock += nextHeader->blockSize;
                nextHeader = (BlockHeader *)nextBlock;
            }

            // Update largest block information
            if (header->blockSize > largestBlockSize) {
                largestBlockSize = header->blockSize;
                largestBlock = block;
            }
        }

        // Move to the next block
        block += header->blockSize;

        // Update the last block information
        if (header->blockSize > MALLOC_BLOCK_HEADER_LENGTH) {
            lastBlock = block;
        }
        break;
    }

    // Update global variables for the biggest and last blocks
    biggestBlock = largestBlock;
}

static void split(void *block, uint32_t nbytes) {
    BlockHeader *header = (BlockHeader *)block;

    if (header->blockSize > nbytes + MALLOC_BLOCK_HEADER_LENGTH) {
        // Calculate the size of the remaining free block
        uint32_t remainingBlockSize = header->blockSize - (nbytes + MALLOC_BLOCK_HEADER_LENGTH);

        // Update the size of the allocated block
        header->blockSize = nbytes + MALLOC_BLOCK_HEADER_LENGTH;

        // Update the next block's header to reflect the remaining free block
        void *nextBlock = block + nbytes + MALLOC_BLOCK_HEADER_LENGTH;
        BlockHeader *nextHeader = (BlockHeader *)nextBlock;

        nextHeader->flags = 0;  // Set the flag for a free block
        nextHeader->blockSize = remainingBlockSize;

        LOG("Block split, allocated "); LOG_INT(nbytes); LOG(" bytes, saved "); LOG_INT(remainingBlockSize); LOG("\n");
    } else {
        LOG("Not enough excess to split the block\n");
    }

    header->flags = 1;
    header->blockSize = nbytes + MALLOC_BLOCK_HEADER_LENGTH;
}

void setReservedMemory(void *begin, void *end) {
    // Implementation needed if required
}

void printMemoryInfo() {
    void *currentBlock = MALLOC_BEGIN_ADDR;
    BlockHeader *header;

    LOG("Memory Blocks Information:\n");
    uint32_t i = 0;
    while (true) {
        header = (BlockHeader *)currentBlock;

        // Check for the end of memory
        if (header->blockSize <= MALLOC_BLOCK_HEADER_LENGTH) {
            break;
        }

        LOG("  Block "); LOG_INT(++i); LOG(" at *"); LOG_INT(currentBlock); LOG("  :  ");
        LOG_INT(header->blockSize); LOG(" (");
        LOG_INT(header->blockSize - MALLOC_BLOCK_HEADER_LENGTH); LOG(") bytes, ");

        if (header->flags != 0) {
            LOG("allocated\n");
        } else {
            LOG("free\n");
        }

        // Move to the next block
        currentBlock += header->blockSize;
    }

    LOG("\n");
}