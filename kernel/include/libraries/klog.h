#ifndef KLOG_H
#define KLOG_H

#include <stdint.h>

void klog(const char* fmt, ...);
void DEBUG(const char* fmt, ...);
void set_debug_mode();
void set_log_level(uint8_t level);

#endif