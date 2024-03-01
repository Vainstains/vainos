#include "../cpu/isr.h"
#include "../cpu/idt.h"
#include "../cpu/utils.h"
#include "../cpu/timer.h"
#include "../drivers/keyboard.h"
#include "../libc/stream.h"
#include "../libc/mem.h"
#include "../drivers/vga.h"
#include "../drivers/disk.h"
#include "../drivers/ports.h"
#include "../filesystem/filesystem.h"
#include "../filesystem/fat16.h"

#include "../types.h"

void reboot()
{
    halt();
    uint8_t good = 0x02;
    while (good & 0x02)
        good = portByteIn(0x64);
    portByteOut(0x64, 0xFE);
    uint64_t target = getTicksSinceBoot() + 200;
    while (getTicksSinceBoot() < target);
    halt();
}

void doubleFaultHandler(registers_t r) {
    vgaWriteln("WARNING: DOUBLE FAULT");
    while(1);
}

void main() {
    isr_install();

    asm volatile("sti");
    init_timer(500);

    init_keyboard();

    register_interrupt_handler(8, doubleFaultHandler);

    vgaClear();
    vgaWriteln("Booted successfully");

    DiskInfo diskInfo;
    diskGetATAPIO(0, &diskInfo);
    Fat16FilesystemInfo fat16info;
    fat16Setup(&diskInfo, &fat16info);
    FSInfo fs;
    fsInit(&fs, (void*)(&fat16info), FILESYSTEM_BACKEND_FAT16);
    fsCreateDirectory(&fs, "/system");
    if (!fsPathExists(&fs, "/system/boot.cfg")) reboot();

    char file[257];
    fsReadFile(&fs, "/system/boot.cfg", file, 256);

    vgaNextLine();
    vgaWriteln(file);
    vgaNextLine();
}