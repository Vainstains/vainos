#if !defined(STRING_H)
#define STRING_H

/* Get the length of a null-terminated string */
int strlen(char s[]);

/* Reverse a null-terminated string */
void reverse(char s[]);

/* Convert an integer to a null-terminated string, represented in decimal */
void int_to_ascii(int n, char str[]);

/* Compare two null-terminated strings */
int strcmp(char s1[], char s2[]);

/* "Append" a character to a null-terminated string */
void append(char s[], char n);

/* "Delete" a character from the end of a null-terminated string */
void backspace(char s[]);

/* Return the index of the first occurrence of `c` in `*s`, or -1 if not present */
int findchar(char s[], char c);

#endif // STRING_H