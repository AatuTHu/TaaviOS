#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <stdint.h>

void memcpy(void *dst, const void *src, uint32_t n);
void memset(void *ptr, int value, int size);
int memcmp(const void *s1, const void *s2, size_t n);
int strlen(const char *s);
int strnlen(const char *s, int n);
int strcmp(const char *a, const char *b);
int str_eq(char *a, char *b);
void strcpy(char *dest, const char *src);
void itoa(int n, char *buf);
int atoi(const char *str);
int str_starts_with(const char *str, const char *prefix);
int strcat(char *dst, const char *src, int n);
void trim(char *str);
#endif
