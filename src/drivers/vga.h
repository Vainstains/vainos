#ifndef VGA_H
#define VGA_H
#include "../types.h"

void initVGA();
void vgaModeSupported(uint16_t mode);
void vgaSetVideoMode(uint16_t mode);
void vgaCurrentVideoMode();
void vgaSetPalette(uint8_t *palette24bpp);

#endif