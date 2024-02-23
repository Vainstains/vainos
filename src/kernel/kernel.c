#include "../cpu/isr.h"
#include "../cpu/idt.h"
#include "../drivers/keyboard.h"
#include "../libc/mem.h"
#include "../drivers/vga.h"
#include "../drivers/disk.h"
#include "../filesystem/fat16.h"

#include "../types.h"


void main() {
    isr_install();

    asm volatile("sti");
    init_timer(500);

    init_keyboard();

    vgaClear();
    vgaWriteln("Booted successfully");

    DiskInfo diskInfo;
    diskGetATAPIO(0, &diskInfo);

    vgaWriteln("Drive info: ");
    vgaWriteByte(diskInfo.allOK); vgaNextLine();
    vgaWriteByte(diskInfo.backend); vgaNextLine();
    vgaWriteByte(diskInfo._atapio_id); vgaNextLine();
    vgaWriteInt32(diskInfo.sectors); vgaNextLine();

    Fat16BootSector bootsector;

    fat16ReadBootsector(&diskInfo, &bootsector);

    vgaWriteln("Boot sector info: ");
    vgaWriteInt(bootsector.bytesPerSector);
}