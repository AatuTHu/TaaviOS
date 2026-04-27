#ifndef STRING_H
#define STRING_H

#include <stdint.h>
#include <stddef.h>

void memcpy(void *dst, const void *src, uint32_t n);
void memset(void *ptr, int value, int size);
int strlen(const char *s);
int strcmp(const char *a, const char *b);
int str_eq(char *a, char *b);
void itoa(int n, char *buf);
int str_starts_with(const char *str, const char *prefix);

#endif