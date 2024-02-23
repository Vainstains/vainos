#include "../cpu/isr.h"
#include "../cpu/idt.h"
#include "../drivers/keyboard.h"
#include "../libc/mem.h"
#include "../drivers/vga.h"
#include "../drivers/disk.h"


void main() {
    isr_install();

    asm volatile("sti");
    init_timer(500);

    init_keyboard();

    vgaClear();
    vgaWriteln("Booted successfully");

    DiskInfo drive;
    getDiskATAPIO(0, &drive);

    vgaWriteln("Drive info: ");
    vgaWriteByte(drive.allOK); vgaNextLine();
    vgaWriteByte(drive.backend); vgaNextLine();
    vgaWriteByte(drive._atapio_id); vgaNextLine();
    vgaWriteInt32(drive.sectors); vgaNextLine();
}