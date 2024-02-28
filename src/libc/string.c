#include "string.h"

int strlen(char s[]) {
    int i = 0;
    while (s[i] != '\0') ++i;
    return i;
}

void reverse(char s[]) {
    int c, i, j;
    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

void int32_to_ascii(uint32_t val, char str[]) {
    int i = 0;
    do {
        str[i++] = val % 10 + '0';
    } while ((val /= 10) > 0);

    str[i] = '\0';

    reverse(str);
}

void int_to_ascii(int n, char str[]) {
    int i, sign;
    if ((sign = n) < 0) n = -n;
    i = 0;
    do {
        str[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);

    if (sign < 0) str[i++] = '-';
    str[i] = '\0';

    reverse(str);
}

void byte_to_ascii(uint8_t byte, char str[]) {
    int i = 0;
    do {
        str[i++] = byte % 10 + '0';
    } while ((byte /= 10) > 0);

    str[i] = '\0';

    reverse(str);
}

int strcmp(char s1[], char s2[]) {
    int i;
    for (i = 0; s1[i] == s2[i]; i++) {
        if (s1[i] == '\0') return 0;
    }
    return s1[i] - s2[i];
}

char* strchr(char *str, char c) {
    int i = 0;
    while (str[i] != '\0') {
        ++i;
        if (str[i] == c) {
            return str + i;
        }
    }
    return str;
}

bool strcontains(char *str, char c) {
    int i = 0;
    while (str[i] != '\0') {
        ++i;
        if (str[i] == c) {
            return true;
        }
    }
    return false;
}

void append(char s[], char n) {
    int len = strlen(s);
    s[len] = n;
    s[len+1] = '\0';
}

char backspace(char s[]) {
    int len = strlen(s);
    if (len == 0) {
        return 0;
    }
    s[len-1] = '\0';
    return 1;
}

int findchar(char s[], char c) {
    int i;
    for (i = 0; s[i] != '\0'; i++) {
        if (s[i] == c) {
            return i;
        }
    }
    return -1;
}