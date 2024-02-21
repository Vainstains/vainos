#include "vga.h"
#include "ports.h"

#define VGA_CRT 0
#define VGA_ACT 1
#define VGA_GEN 2
#define VGA_SEQ 3
#define VGA_GCT 4
#define VGA_OLD 5

void initVGA() {

}

void vgaWriteReg(uint8_t set, uint8_t idx, uint8_t value) {
    switch (set) {
        case VGA_CRT:
            if (idx <= 24) {
                portByteOut(0x3d4, idx); // crt addr
                portByteOut(0x3d5, value); // crt data
            }
            break;
        case VGA_ACT:
            if (idx <= 20) {
                portByteIn(0x3da); // gen status
                idx = (portByteIn(0x3c0)&0x20)|idx; // act adda
                portByteIn(0x3da); // gen status
                portByteOut(0x3c0, idx); // act adda
                portByteOut(0x3c0, value); // act adda
            }
            break;
        case VGA_GEN:
            if (idx == 0) {
                portByteOut(0x3c2, value); // gen misc w
            }
            break;
        case VGA_SEQ:
            break;
        case VGA_GCT:
            break;
        case VGA_OLD:
            break;
    }
}

uint8_t vgaReadReg(uint8_t set, uint8_t idx) {
    
}

void vgaModeSupported(uint16_t mode) {

}

void vgaSetVideoMode(uint16_t mode) {

}

void vgaCurrentVideoMode() {

}

void vgaSetPalette(uint8_t *palette24bpp) {

}