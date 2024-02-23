#ifndef ATAPIO_H
#define ATAPIO_H

#include "../types.h"

// Ports

#define ATAPIO_Identify_Primary 0xA0
#define ATAPIO_Identify_Secondary 0xB0

#define ATAPIO_ReadWrite28_Primary 0xE0
#define ATAPIO_ReadWrite28_Secondary 0xF0

#define ATAPIO_Port_Data 0x1F0
#define ATAPIO_Port_Error 0x1F1
#define ATAPIO_Port_SectorCount 0x1F2
#define ATAPIO_Port_LBAlo 0x1F3
#define ATAPIO_Port_LBAmid 0x1F4
#define ATAPIO_Port_LBAhi 0x1F5
#define ATAPIO_Port_DriveSelect 0x1F6
#define ATAPIO_Port_CommStat 0x1F7

/* Use ATAPIO_Identify_<Primary/Secondary> as the target */
bool atapioIdentify(uint8_t target, uint16_t *buffer);

/* Use ATAPIO_ReadWrite28_<Primary/Secondary> as the target */
void atapioRead28(uint8_t target, uint32_t LBA, uint8_t sectorCount, uint8_t *buffer);

/* Use ATAPIO_ReadWrite28_<Primary/Secondary> as the target */
void atapioWrite28(uint8_t target, uint32_t LBA, uint8_t sectorCount, const uint8_t *buffer);

#endif // ATAPIO_H