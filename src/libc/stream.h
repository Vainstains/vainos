#ifndef STREAM_H
#define STREAM_H

#include "../types.h"

typedef struct {
    uint16_t bytesAvailable;
    uint16_t bufferSize;
    char *buffer;
    char (*_next)(void*);
    uint32_t _pos;
    uint32_t consumed;
} ByteStream;

#endif // STREAM_H