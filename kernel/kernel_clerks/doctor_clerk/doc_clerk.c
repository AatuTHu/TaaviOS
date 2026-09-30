#include "doc_clerk.h"
#include "blankie.h"
#include "config.h"
#include "hail_mary.h"
#include "klog.h"
#include "ledger.h"
#include "sched.h"

static int doc_handle_req(request_table *req) {
    task_t *doc_clerk = task_get(doc_clerk_pid);

    if (doc_clerk == NULL) {
        ERROR("[DOC][HANDLE_REQ]: Could not find doc clerk. Aborting\n");
        return STATUS_ERROR;
    }

    switch (req->request_type) {
    case TEST_CREATE_WINDOW:
        doc_test_create_window(doc_clerk, req);
        break;
    case TEST_RESIZE_WINDOW:
        doc_test_resize_window(doc_clerk, req);
        break;
    case TEST_MOVE_WINDOW:
        doc_test_move_window(doc_clerk, req);
        break;
    default:
        ERROR("[DOC][HANDLE_REQUEST]: invalid request type\n");
        req->status = FAILED;
        scheduler_wake_task(req->caller_pid);
        return STATUS_ERROR;
    }

    return STATUS_OK;
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
