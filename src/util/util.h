#ifndef UTIL_H
#define UTIL_H

/* copy `nbytes` bytes to `source`, starting at `dest` */
void memoryCopy(char *source, char *dest, int nbytes);

void memorySet(char *dest, char val, int nbytes);

int strlen(char s[]);
void reverse(char s[]);
void int_to_ascii(int n, char str[]);
int strcmp(char s1[], char s2[]);
void append(char s[], char n);
void backspace(char s[]);
#endif