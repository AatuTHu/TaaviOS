#ifndef KPRINTF_H
#define KPRINTF_H

#include <stdint.h>

void klog(uint8_t level,const char* fmt, ...);

#endif