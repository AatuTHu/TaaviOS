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

typedef struct {
    request_table **table;
    int max_entries;
    int *last_idx;
} clerk_queue;

void ledger_check_request(uint32_t clerk_pid);
int ledger_collect(uint32_t caller_pid, uint32_t clerk_pid, char *out);
int ledger_add_fs_req(uint32_t caller_pid,
    operations_t type, uint32_t fd, const char *path,
    const char *buf, uint32_t buffer_size, uint32_t flags);
int ledger_add_gui_req(uint32_t caller_pid, operations_t type, const char *buf, uint32_t buffer_size);
request_table *ledger_fetch_next_req(uint32_t clerk_pid);
void ledger_init();
int ledger_remove_request();

#endif