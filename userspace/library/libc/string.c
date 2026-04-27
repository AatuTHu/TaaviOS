#include "string.h"

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
    int i = 0;
    while (s[i]) i++;
    return i;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

int str_eq(char *a, char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

int str_starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*str++ != *prefix++) return 0;
    }
    return 1;
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