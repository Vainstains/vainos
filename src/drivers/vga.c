#include "../drivers/ports.h"
#include "../libc/mem.h"
#include "../libc/string.h"

#define REG_SCREEN_CTRL 0x3d4
#define REG_SCREEN_DATA 0x3d5

#define VGA_ADDRESS 0xb8000

#define MAX_ROWS 25
#define MAX_COLS 80

int vgaGetOffset(int col, int row) {
    return 2 * (row * MAX_COLS + col);
}

int vgaGetOffsetRow(int offset) {
    return offset / (2 * MAX_COLS); 
}

int vgaGetOffsetCol(int offset) {
    return (offset - (vgaGetOffsetRow(offset)*2*MAX_COLS))/2; 
}

int vgaGetCursor() {
    portByteOut(0x3d4, 14);
    int position = portByteIn(0x3d5);
    position = position << 8;
    portByteOut(0x3d4, 15);
    position += portByteIn(0x3d5);
    return position * 2;
}

void vgaSetCursor(int offset) {
    offset /= 2;
    portByteOut(REG_SCREEN_CTRL, 14);
    portByteOut(REG_SCREEN_DATA, (unsigned char)(offset >> 8));
    portByteOut(REG_SCREEN_CTRL, 15);
    portByteOut(REG_SCREEN_DATA, (unsigned char)(offset & 0xff));
}

void vgaNextLine() {
    vgaWriteChar('\n');
}

void vgaWriteChar(char c) {
    int cursor = vgaGetCursor();
    if (c == '\n') {
        cursor /= 2 * MAX_COLS; // reset to beginning of line
        cursor *= 2 * MAX_COLS;

        cursor += 2 * MAX_COLS; // next line
    } else {
        char *vga = (char*)VGA_ADDRESS;
        vga[cursor] = c; 
        vga[cursor+1] = 0x0f;
        cursor += 2;
    }

    if (cursor >= MAX_ROWS * MAX_COLS * 2) {
        int i;
        for (i = 1; i < MAX_ROWS; i++) 
            memcpy(vgaGetOffset(0, i) + VGA_ADDRESS,
                        vgaGetOffset(0, i-1) + VGA_ADDRESS,
                        MAX_COLS * 2);

        /* Blank last line */
        char *last_line = vgaGetOffset(0, MAX_ROWS-1) + VGA_ADDRESS;
        for (i = 0; i < MAX_COLS * 2; i++) last_line[i] = 0;

        cursor -= 2 * MAX_COLS;
    }
    vgaSetCursor(cursor);
    
    return;
}

void vgaWrite(char *string) {
    int i = 0;
    while (string[i] != 0) {
        vgaWriteChar(string[i]);
        i++;
    }
}

void vgaWriteStatic(char *string, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        if (string[i] == '\0')
        {
            vgaWriteChar('*');
            continue;
        }
        vgaWriteChar(string[i]);
    }
}

void vgaWriteln(char *string) {
    int i = 0;
    while (string[i] != 0) {
        vgaWriteChar(string[i]);
        i++;
    }
    vgaNextLine();
}

void vgaWriteBackspace() {
    int cursor = vgaGetCursor();
    char *vga = (char*)VGA_ADDRESS;
    vga[cursor-2] = 0x00; 
    vga[cursor-1] = 0x0f;
    vgaSetCursor(cursor-2);
}

void vgaWriteInt32(uint32_t num) {
    char buffer[256];
    int32_to_ascii(num, buffer);

    vgaWrite(buffer);
}

void vgaWriteInt(int num) {
    char buffer[64];
    
    int_to_ascii(num, buffer);
    
    vgaWrite(buffer);
}

void vgaWriteByte(uint8_t num) {
    char buffer[4];
    byte_to_ascii(num, buffer);
    buffer[3] = '\0';
    vgaWrite(buffer);
}

void vgaClear() {
    char *lastLine = (char*)VGA_ADDRESS;
    for (int i = 0; i < MAX_COLS * MAX_ROWS * 2; i++)
    {
        lastLine[i] = 0;
    }
    vgaSetCursor(0);
}