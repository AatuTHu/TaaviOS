#include "doc_clerk.h"
#include "blankie.h"
#include "config.h"
#include "hail_mary.h"
#include "klog.h"
#include "kstring.h"
#include "ledger.h"
#include "sched.h"
#include "shared.h"
#include "task.h"
#include <stdint.h>

static int window_key = -1;

static int test_create_window() {
    gui_params_pack params;
    memset(&params, 0, sizeof(params));

    task_t *doc_clerk = task_get(doc_clerk_pid);

    if (doc_clerk == NULL) {
        ERROR("[DOC][CREATE_WINDOW]: Failed to fetch self. TEST FAILED\n");
        return STATUS_ERROR;
    }

    params.opcode   = CREATE;
    params.width    = 300;
    params.height   = 300;
    params.x        = 300;
    params.y        = 300;
    params.fg_color = COLOR_LIGHT_GRAY;
    params.bg_color = COLOR_BLACK;

    if (ledger_add_gui_req(doc_clerk_pid, &params) == STATUS_ERROR) {
        ERROR("[DOC][CREATE_WINDOW]: Failed to add request. TEST FAILED\n");
    }
    doc_clerk->state = TASK_BLOCKED;
    scheduler_yield(&doc_clerk->context);

    window_key = ledger_collect(doc_clerk_pid, gui_task_pid, NULL);

    if (window_key == STATUS_ERROR) {
        ERROR("[DOC][CREATE_WINDOW]: Failed to create window. TEST FAILED\n");
        return STATUS_ERROR;
    }

    DEBUG_DOC("[DOC][CREATE_WINDOW]: TEST PASSED\n");
    return STATUS_OK;
}

static int test_delete_window() {

    task_t *doc_clerk = task_get(doc_clerk_pid);

    if (doc_clerk == NULL) {
        ERROR("[DOC][DELETE_WINDOW]: Failed to fetch self. TEST FAILED\n");
        return STATUS_ERROR;
    }

    doc_clerk->state = TASK_BLOCKED;
    if (ledger_queue_free_req(doc_clerk_pid, gui_task_pid, doc_clerk_pid) == STATUS_ERROR) {
        ERROR("[DOC][DELETE_WINDOW]: Failed to queue request. TEST FAILED\n");
    }
    scheduler_yield(&doc_clerk->context);

    int delete_result = ledger_collect(doc_clerk_pid, gui_task_pid, NULL);

    if (delete_result == STATUS_ERROR) {
        ERROR("[DOC][DELETE_WINDOW]: Failed to DELETE window. TEST_FAILED\n");
        return STATUS_ERROR;
    }

    window_key = -1;

    DEBUG_DOC("[DOC][DELETE_WINDOW]: TEST PASSED\n");
    return STATUS_OK;
}

static int doc_handle_req(request_table *req) {

    task_t *doc_clerk = task_get(doc_clerk_pid);

    if (doc_clerk == NULL) {
        ERROR("[DOC][HANDLE_REQ]: Could not find doc clerk. Aborting\n");
        return STATUS_ERROR;
    }

    switch (req->request_type) {
    case RUN_ALL_TESTS: {
        if (test_create_window() == STATUS_ERROR) {
            break;
        }

        if (test_delete_window() == STATUS_ERROR) {
            break;
        }

        req->status         = COMPLETE;
        doc_clerk->priority = PRIORITY_NORMAL;
        scheduler_wake_task(req->caller_pid);
        return STATUS_OK;
    }
    default:
        ERROR("[DOC][HANDLE_REQUEST]: invalid request type\n");
        req->status = FAILED;
        scheduler_wake_task(req->caller_pid);
        return STATUS_ERROR;
    }

    req->status = FAILED;
    scheduler_wake_task(req->caller_pid);
    return STATUS_ERROR;
}

void doc_clerk_loop() {
    DEBUG_DOC("[DOC][DOC_LOOP]\n");
    while (1) {
        request_table *req = ledger_fetch_next_req(doc_clerk_pid);

        if (req != NULL) {
            if (req->status == PENDING || req->status == IN_PROGRESS) {
                doc_handle_req(req);
            }
        }

        blankie_activate(doc_clerk_pid);
    }
}

static void doc_recovery() {
    ERROR("[DOC][RECOVERY]:\n");
    ledger_check_request(doc_clerk_pid);
    blankie_activate(doc_clerk_pid);
}

void doc_init(const task_t *doc_clerk) {
    blankie_register(doc_clerk_pid, doc_clerk->context.eip, doc_clerk->kernel_stack);
    register_hail_mary_function(doc_clerk_pid, doc_recovery);
}
