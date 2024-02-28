#include "../cpu/isr.h"
#include "../cpu/idt.h"
#include "../drivers/keyboard.h"
#include "../libc/stream.h"
#include "../libc/mem.h"
#include "../drivers/vga.h"
#include "../drivers/disk.h"
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

    vgaWriteln("Drive info: ");
    
    vgaWriteByte(diskInfo.allOK); vgaWrite(" < aaa"); vgaNextLine();
    vgaWriteByte(diskInfo.backend); vgaNextLine();
    
    vgaWriteByte(diskInfo._atapio_id); vgaNextLine();
    vgaWriteInt(diskInfo.sectors); vgaNextLine();

    Fat16FilesystemInfo fs;
    fat16Setup(&diskInfo, &fs);

    vgaNextLine();
    fat16CreateFolder(&fs, "hello");
    vgaNextLine();

    //printMemoryInfo();
    
    //while(1);


    fat16CreateFolder(&fs, "hello/world");
    vgaNextLine();
    fat16CreateFolder(&fs, "world/hello");
    vgaNextLine();
    fat16CreateFolder(&fs, "abc/def");
    //vgaNextLine();

    //vgaWriteln("Checkpoint :)");
}