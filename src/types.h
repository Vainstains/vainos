#ifndef INCLUDE_TYPES_H
#define INCLUDE_TYPES_H

typedef unsigned char int8;
typedef int8 byte;
typedef unsigned short int16;
typedef int16 byte2;
typedef unsigned long int32;
typedef int16 byte4;
typedef unsigned long long int64;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;

typedef unsigned int size_t;

typedef void (*funcptr_base_t)(void);
#define NULL ((void*)0)

#define bool char
#define true (char)1
#define false (char)0

#define low_16(address) (uint16_t)((address) & 0xFFFF)
#define high_16(address) (uint16_t)(((address) >> 16) & 0xFFFF)

#endif /* INCLUDE_TYPES_H */