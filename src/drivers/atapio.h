#ifndef ATAPIO_H
#define ATAPIO_H

#include "../types.h"

// Ports

#define ATAPIO_Port_DriveSelect 0x1F6
#define ATAPIO_Port_SectorCount 0x1F2

#define ATAPIO_Port_LBAlo 0x1F3
#define ATAPIO_Port_LBAmid 0x1F4
#define ATAPIO_Port_LBAhi 0x1F5

#define ATAPIO_Port_CommStat 0x1F7

bool atapioIdentify(uint8_t target, uint16_t *data);

#endif // ATAPIO_H