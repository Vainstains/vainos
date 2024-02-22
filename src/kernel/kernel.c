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
    bool a = atapioIdentify(0, idBuf);
    vgaWriteByte(a);
}