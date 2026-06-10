#ifndef LEDGER_H
#define LEDGER_H
#include "config.h"
#include "klog.h"
#include "kmalloc.h"
#include "kstring.h"
#include "sched.h"

typedef enum {
    PENDING,
    IN_PROGRESS,
    COMPLETE,
    TERMINATED,
    FAILED,
} reqistry_status;

typedef struct request_table {
    uint32_t caller_pid;
    uint32_t clerk_pid;
    operations_t request_type;
    char path[128];
    char buf[512];
    uint32_t buffer_size;
    uint32_t fd;
    uint32_t flags;
    reqistry_status status;
} request_table;

int collect_request(uint32_t caller_pid, uint32_t clerk_pid, char *out);
int add_request_to_ledger(uint32_t caller_pid, uint32_t clerk_pid,
    operations_t type, uint32_t fd, const char *path,
    const char *buf, uint32_t buffer_size, uint32_t flags);
request_table *fetch_next_task(uint32_t clerk_pid);
void ledger_init();

extern request_table *request_queue[CLERK_COUNT][MAX_REQ_ENTRIES];

#endif