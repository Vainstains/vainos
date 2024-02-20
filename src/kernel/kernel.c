#include "../drivers/vga.h"
#include "../cpu/isr.h"
#include "../cpu/idt.h"
#include "../drivers/keyboard.h"
#include "../libc/mem.h"

void memtest() {
    vgaWriteln("\nTesting `malloc` and `dealloc`...\n");
    
    void* mem1 = malloc(4);
    mem1 = malloc(4);
    mem1 = malloc(4);

    printMemoryInfo();

    vgaWriteln("\nDone testing `malloc` and `dealloc`.\n");
}

void main() {
    vgaWriteln("Loading...");

    isr_install();

    asm volatile("sti");
    init_timer(500);

    memtest();

    init_keyboard();
}