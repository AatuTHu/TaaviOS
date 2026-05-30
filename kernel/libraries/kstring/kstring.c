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

int strlen(const char *s) {
  int len = 0;
  while(*s != '\0') {
    len++;
    s++;
  }
  return len;
}

void strcpy(char *dest, char *src) {
  while(*src != '\0') {
    *dest = *src;
    src++;
    dest++;
  }
  *dest = '\0';
}

void strncpy(char *dest,const char *src, uint32_t size) {
  uint32_t i;
  for(i = 0; i < size-1; i++) {
    if(src[i] == '\0') break;
    dest[i] = src[i];
  }
  dest[i] = '\0';
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

void itoa(int n, char *buf) {
    int i = 0;
    int isNegative = 0;

     if (n == 0) {
        buf[i++] = '0';
        buf[i] = '\0';
        return;
    }
    
    if (n < 0) {
        isNegative = 1;
        n = -n;
        buf[i++] = '-';
    }

    while (n != 0) {
        int rem = n % 10;
        buf[i++] = rem + '0';
        n /= 10;
    }

    int start = isNegative ? 1 : 0;
    int end = i - 1;
    while (start < end) {
        char tmp = buf[start];
        buf[start] = buf[end];
        buf[end] = tmp;
        start++;
        end--;
    }

    buf[i] = '\0';
}