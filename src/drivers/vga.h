#ifndef VGA_H
#define VGA_H
#include "../types.h"


int vgaGetOffset(int col, int row);
int vgaGetOffsetRow(int offset);
int vgaGetOffsetCol(int offset);
int vgaGetCursor();
void vgaSetCursor(int offset);
void vgaNextLine();
void vgaWriteChar(char c);
void vgaWrite(char *string);
void vgaWriteln(char *string);
void vgaWriteBackspace();
void vgaWriteInt(int num);
void vgaWriteByte(uint8_t num);
void vgaClear();

#endif