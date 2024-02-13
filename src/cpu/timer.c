#include "timer.h"
#include "../drivers/vga.h"
#include "../util/util.h"
#include "isr.h"

uint32_t tick = 0;

static void timer_callback(registers_t regs) {
    tick++;
}

void init_timer(uint32_t freq) {
    /* Install the function we just wrote */
    register_interrupt_handler(IRQ0, timer_callback);

    /* Get the PIT value: hardware clock at 1193180 Hz */
    uint32_t divisor = 1193180 / freq;
    uint8_t low  = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)( (divisor >> 8) & 0xFF);
    /* Send the command */
    portByteOut(0x43, 0x36); /* Command port */
    portByteOut(0x40, low);
    portByteOut(0x40, high);
}