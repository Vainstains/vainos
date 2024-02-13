#include "keyboard.h"
#include "ports.h"
#include "../cpu/isr.h"
#include "vga.h"

#define BACKSPACE 0x0E
#define ENTER 0x1C
#define LSHIFT 0x2A
#define RSHIFT 0x36

static char key_buffer[256];
static int shift_pressed = 0;

#define SC_MAX 57
const char *sc_name[] = { "ERROR", "Esc", "1", "2", "3", "4", "5", "6", 
    "7", "8", "9", "0", "-", "=", "Backspace", "Tab", "Q", "W", "E", 
        "R", "T", "Y", "U", "I", "O", "P", "[", "]", "Enter", "Lctrl", 
        "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "`", 
        "LShift", "\\", "Z", "X", "C", "V", "B", "N", "M", ",", ".", 
        "/", "RShift", "Keypad *", "LAlt", "Spacebar"};

const char sc_ascii[] = {'?', '?', '1', '2', '3', '4', '5', '6',
                         '7', '8', '9', '0', '-', '=', '?', '?', 'q', 'w', 'e', 'r', 't', 'y',
                         'u', 'i', 'o', 'p', '[', ']', '?', '?', 'a', 's', 'd', 'f', 'g',
                         'h', 'j', 'k', 'l', ';', '\'', '`', '?', '\\', 'z', 'x', 'c', 'v',
                         'b', 'n', 'm', ',', '.', '/', '?', '?', '?', ' '};

const char sc_shifted_ascii[] = {'?', '?', '!', '@', '#', '$', '%', '^',
                                '&', '*', '(', ')', '_', '+', '?', '?', 'Q', 'W', 'E', 'R', 'T', 'Y',
                                'U', 'I', 'O', 'P', '{', '}', '?', '?', 'A', 'S', 'D', 'F', 'G',
                                'H', 'J', 'K', 'L', ':', '"', '~', '?', '|', 'Z', 'X', 'C', 'V',
                                'B', 'N', 'M', '<', '>', '?', '?', '?', ' '};

static void keyboard_callback(registers_t regs) {
    /* The PIC leaves us the scancode in port 0x60 */
    uint8_t scancode = portByteIn(0x60);

    if (scancode == LSHIFT || scancode == RSHIFT) {
        shift_pressed = 1;
        return;
    } else if (scancode == (LSHIFT | 0x80) || scancode == (RSHIFT | 0x80)) {
        shift_pressed = 0;
        return;
    }
    if (scancode > SC_MAX) return;
    if (scancode == BACKSPACE) {
        backspace(key_buffer);
        vgaWriteBackspace();
    } else if (scancode == ENTER) {
        vgaWrite("\n");
        user_input(key_buffer); /* kernel-controlled function */
        key_buffer[0] = '\0';
    } else {
        char letter = shift_pressed?sc_shifted_ascii[(int)scancode]:sc_ascii[(int)scancode];
        /* Remember that kprint only accepts char[] */
        char str[2] = {letter, '\0'};
        append(key_buffer, letter);
        vgaWrite(str);
    }
}
void user_input(char *input) {
    if (strcmp(input, "END") == 0) {
        vgaWrite("Stopping the CPU. Bye!\n");
        asm volatile("hlt");
    }
    vgaWrite("You said: ");
    vgaWrite(input);
    vgaWrite("\n> ");
}
void init_keyboard() {
   register_interrupt_handler(IRQ1, keyboard_callback);
   vgaWrite("Type something, it will go through the kernel\n"
        "Type END to halt the CPU\n> ");
}