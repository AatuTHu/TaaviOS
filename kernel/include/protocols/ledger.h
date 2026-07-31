#ifndef LEDGER_H
#define LEDGER_H
#include "shared.h"
#include <stdint.h>

typedef enum {
    PENDING,
    IN_PROGRESS,
    COMPLETE,
    TERMINATED,
    FAILED,
} reqistry_status;

typedef struct request_table {
    char *buf;
    uint32_t *pixels;
    uint32_t fg_color;
    uint32_t bg_color;
    uint32_t buffer_size;
    uint32_t fd;
    uint32_t flags;
    uint16_t caller_pid;
    uint16_t target_pid;
    uint16_t width;
    uint16_t height;
    uint16_t x;
    uint16_t y;
    uint8_t status;
    uint8_t request_type;
    uint8_t clerk_pid;
    uint8_t scale;
} request_table;

typedef struct {
    request_table **table;
    int max_entries;
    int *last_idx;
} clerk_queue;

void ledger_check_request(uint32_t clerk_pid);
int ledger_collect(uint32_t caller_pid, uint32_t clerk_pid, char *out);
// fs
int ledger_add_fs_req(uint32_t caller_pid,
                      operations_t type, uint32_t fd,
                      const char *buf, uint32_t buffer_size, uint32_t flags);
int ledger_add_fs_free_req(uint32_t caller_pid, uint32_t target_pid);
// gui
int ledger_add_gui_req(uint32_t caller_pid, gui_params_pack *params);
int ledger_add_gui_free_req(uint32_t caller_pid, uint32_t target_pid);
// reaper
int ledger_add_reaper_req(uint32_t caller_pid, uint32_t target_pid);

request_table *ledger_fetch_next_req(uint32_t clerk_pid);
void ledger_init();
void ledger_remove_request();
int ledger_count_clerk_reqs(uint32_t clerk_pid);
int ledger_count_active_reqs();
int ledger_has_killable_reqs();
int ledger_enqueue(uint32_t clerk_pid, request_table *req);

#endif
