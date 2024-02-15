#if !defined(UTIL_H)
#define UTIL_H

/* copy `nbytes` bytes to `*source`, starting at `*dest` */
void memoryCopy(char *source, char *dest, int nbytes);

/* set the value of `nbytes` bytes to `val`, starting at `*dest` */
void memorySet(char *dest, char val, int nbytes);

#endif // UTIL_H