#ifndef LEDGER_H
#define LEDGER_H
#include "config.h"
#include "klog.h"
#include "kmalloc.h"
#include "kstring.h"
#include "sched.h"
#include "shared.h"

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
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
    uint32_t color;
    reqistry_status status;
    uint32_t *pixels;
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
int ledger_add_gui_req(uint32_t caller_pid, operations_t type, uint32_t width, uint32_t height,
                       uint32_t x, uint32_t y, const char *buf, uint32_t buffer_size, uint32_t color, const uint32_t *user_pixels);
request_table *ledger_fetch_next_req(uint32_t clerk_pid);
void ledger_init();
int ledger_remove_request();
int ledger_count_clerk_reqs(uint32_t clerk_pid);
int ledger_count_active_reqs();
int ledger_has_killable_reqs();
int ledger_enqueue(uint32_t clerk_pid, request_table *req);

#endif