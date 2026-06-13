#ifndef LOG_H
#define LOG_H

#define LOG_NONE       0
#define LOG_ERROR      (1 << 0) // 1
#define LOG_ALL_DEBUGS (1 << 1) // 2
#define LOG_SCHED      (1 << 2) // 4
#define LOG_KMALLOC    (1 << 3) // 8
#define LOG_FS_TASK    (1 << 4) // 16
#define LOG_FAT32      (1 << 5) // 32
#define LOG_LEDGER     (1 << 6) // 64
#define LOG_PAGING     (1 << 7) // 128
#define LOG_TASK       (1 << 8) // 256

#define LOG_FS_ALL (LOG_FAT32 | LOG_FS_TASK | LOG_LEDGER)
#define LOG_MM_ALL (LOG_KMALLOC | LOG_PAGING)
#define LOG_ALL    (0xFFFFFFFF)

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_ERROR
#endif

#if LOG_LEVEL == LOG_NONE
#define ERROR(fmt, ...) ((void)0)
#else
#define ERROR(fmt, ...) klog_error(fmt, ##__VA_ARGS__)
#endif

#if (LOG_LEVEL & LOG_ALL_DEBUGS) || (LOG_LEVEL == LOG_ALL)
#define DEBUG(fmt, ...) klog_debug(fmt, ##__VA_ARGS__)
#else
#define DEBUG(fmt, ...) ((void)0)
#endif

#if (LOG_LEVEL & LOG_SCHED) || (LOG_LEVEL & LOG_ALL_DEBUGS)
#define DEBUG_SCHED(fmt, ...) klog_debug(fmt, ##__VA_ARGS__)
#else
#define DEBUG_SCHED(fmt, ...) ((void)0)
#endif

#if (LOG_LEVEL & LOG_KMALLOC) || (LOG_LEVEL & LOG_ALL_DEBUGS)
#define DEBUG_KMALLOC(fmt, ...) klog_debug(fmt, ##__VA_ARGS__)
#else
#define DEBUG_KMALLOC(fmt, ...) ((void)0)
#endif

#if (LOG_LEVEL & LOG_FS_TASK) || (LOG_LEVEL & LOG_ALL_DEBUGS)
#define DEBUG_FS_TASK(fmt, ...) klog_debug(fmt, ##__VA_ARGS__)
#else
#define DEBUG_FS_TASK(fmt, ...) ((void)0)
#endif

#if (LOG_LEVEL & LOG_FAT32) || (LOG_LEVEL & LOG_ALL_DEBUGS)
#define DEBUG_FAT32(fmt, ...) klog_debug(fmt, ##__VA_ARGS__)
#else
#define DEBUG_FAT32(fmt, ...) ((void)0)
#endif

#if (LOG_LEVEL & LOG_LEDGER) || (LOG_LEVEL & LOG_ALL_DEBUGS)
#define DEBUG_LEDGER(fmt, ...) klog_debug(fmt, ##__VA_ARGS__)
#else
#define DEBUG_LEDGER(fmt, ...) ((void)0)
#endif

#if (LOG_LEVEL & LOG_PAGING) || (LOG_LEVEL & LOG_ALL_DEBUGS)
#define DEBUG_PAGING(fmt, ...) klog_debug(fmt, ##__VA_ARGS__)
#else
#define DEBUG_PAGING(fmt, ...) ((void)0)
#endif

#if (LOG_LEVEL & LOG_TASK) || (LOG_LEVEL & LOG_ALL_DEBUGS)
#define DEBUG_TASK(fmt, ...) klog_debug(fmt, ##__VA_ARGS__)
#else
#define DEBUG_TASK(fmt, ...) ((void)0)
#endif

#endif