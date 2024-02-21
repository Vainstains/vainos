#include "../cpu/isr.h"
#include "../cpu/idt.h"
#include "../drivers/keyboard.h"
#include "../libc/mem.h"

void main() {
    isr_install();

    asm volatile("sti");
    init_timer(500);

    init_keyboard();
}