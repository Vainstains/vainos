#ifndef VGA_H
#define VGA_H

int vgaGetOffset(int col, int row);
int vgaGetOffsetCol(int offset);
int vgaGetOffsetRow(int offset);
int vgaGetCursor();
void vgaSetCursor(int offset);
void vgaNextLine();
void vgaWriteChar(char c);
void vgaWrite(char* string);
void vgaWriteln(char* string);
void vgaWriteInt(int number);
void vgaWriteByte(char number);
void vgaClear();

#endif