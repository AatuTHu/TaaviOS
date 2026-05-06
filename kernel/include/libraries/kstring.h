#ifndef KSTRING_H
#define KSTRING_H
#include <stdint.h>
#include <stddef.h>

void memcpy(void *dst, const void *src, uint32_t n);
void memset(void *ptr, int value, int size);
int  memcmp(const void *ptr1, const void *ptr2, size_t n);
void strcpy(char *dest, char *src);
void strncpy(char *dest,const char *src, uint8_t size);
int  strlen(char *s);
int  strcmp(const char *a, const char *b);
void itoa(int n, char *buf);


#endif