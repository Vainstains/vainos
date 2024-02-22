#include "../cpu/isr.h"
#include "../cpu/idt.h"
#include "../drivers/keyboard.h"
#include "../libc/mem.h"
#include "../drivers/vga.h"
#include "../drivers/atapio.h"
void main() {
    isr_install();

    asm volatile("sti");
    init_timer(500);

    init_keyboard();

    vgaClear();
    vgaWriteln("Booted successfully");

    uint16_t idBuf[256];
    atapioIdentify(ATAPIO_Identify_Primary, idBuf);
    for (size_t i = 0; i < 16; i++)
    {
        vgaWriteInt((int)idBuf[i]); vgaNextLine();
    }
    printMemoryInfo();
    vgaNextLine();
    vgaNextLine();

    uint8_t readBuf[512];
    atapioRead28(ATAPIO_ReadWrite28_Primary, 0, 1, readBuf);
    for (size_t i = 0; i < 512; i++)
    {
        vgaWriteInt((int)readBuf[i]); vgaNextLine();
    }
}