#include "../drivers/vga.h"
#include "../cpu/isr.h"
#include "../cpu/idt.h"
#include "../drivers/keyboard.h"

void main() {
    vgaWriteln("Loading...");

    isr_install();

    asm volatile("sti");
    init_timer(100);

    init_keyboard();
}