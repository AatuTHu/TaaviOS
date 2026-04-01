#include "kstring.h"

void memcpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t*)dst;
    const uint8_t *s = (const uint8_t*)src;
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];
}

void memset(void *ptr, int value, int size)
{
    char *p = (char *)ptr;
    for(int i= 0; i < size; i++) {
      p[i] = value;
    }
}

int strlen(char *s) {
  int len = 0;
  while(*s != '\0') {
    len++;
    s++;
  }
  return len;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

int memcmp(const void *ptr1, const void *ptr2, size_t n) {
    const unsigned char *s1 = ptr1;
    const unsigned char *s2 = ptr2;

    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) {
            return s1[i] - s2[i];
        }
    }
    return 0;
}