#ifndef DEBUG_H
#define DEBUG_H

#include "debug.h"

#define DEBUG
#ifdef DEBUG

#define LOG(message)  //vgaWrite(message)
#define LOG_INT(num)  //vgaWriteInt(num)
#define LOG_BYTE(num) //vgaWriteByte(num)

#else

#define LOG(message)
#define LOG_INT(num)
#define LOG_BYTE(num)

#endif  // DEBUG

#endif  // DEBUG_H