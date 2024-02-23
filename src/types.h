#ifndef INCLUDE_TYPES_H
#define INCLUDE_TYPES_H

typedef unsigned char uint8_t;
typedef uint8_t byte;
typedef unsigned short uint16_t;
typedef uint16_t word;
typedef unsigned long uint32_t;
typedef unsigned long long uint64_t;

typedef unsigned int size_t;

typedef void (*funcptr_base_t)(void);
#define NULL ((void*)0)

#define bool char
#define true (char)1
#define false (char)0

#define low_16(address) (uint16_t)((address) & 0xFFFF)
#define high_16(address) (uint16_t)(((address) >> 16) & 0xFFFF)

#endif /* INCLUDE_TYPES_H */