#include "../cpu/isr.h"
#include "../cpu/idt.h"
#include "../drivers/keyboard.h"
#include "../libc/stream.h"
#include "../libc/mem.h"
#include "../drivers/vga.h"
#include "../drivers/disk.h"
#include "../filesystem/filesystem.h"
#include "../filesystem/fat16.h"

#include "../types.h"

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

    vgaNextLine();
    fsCreateDirectory(&fs, "/hello");
    fsCreateDirectory(&fs, "/hello");
    fsCreateFile(&fs, "/hello/world.txt");
    fsCreateFile(&fs, "/hello/world.txt");
    fsWriteFile(&fs, "/hello/world.txt", "abc123", 6);
    vgaNextLine();

    char file[20];
    fsReadFile(&fs, "/hello/world.txt", file, 6);
    vgaWriteln(file);
    //printRootDirectory(&fs);
}