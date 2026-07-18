#ifndef KSTRING_H
#define KSTRING_H
#include <stddef.h>
#include <stdint.h>

void memcpy(void *dst, const void *src, uint32_t n);
void memset(void *ptr, int value, int size);
int memcmp(const void *ptr1, const void *ptr2, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void strcpy(char *dest, const char *src);
void strncpy(char *dest, const char *src, uint32_t size);
int strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
void itoa(int n, char *buf);
char *strcat(char *dst, const char *src);
int atoi(const char *str);

#endif