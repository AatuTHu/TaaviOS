#ifndef KLOG_H
#define KLOG_H

#include <stdint.h>

void klog(const char *fmt, ...);
void klog_debug(const char *fmt, ...);
void klog_error(const char *fmt, ...);

#define LOG_NONE  0
#define LOG_ERROR 1
#define LOG_DEBUG 2

#if LOG_LEVEL >= LOG_DEBUG
#define DEBUG(fmt, ...) klog_debug(fmt, ##__VA_ARGS__)
#else
#define DEBUG(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_ERROR
#define ERROR(fmt, ...) klog_error(fmt, ##__VA_ARGS__)
#else
#define ERROR(fmt, ...) ((void)0)
#endif

void set_print_level(uint8_t level);

#endif