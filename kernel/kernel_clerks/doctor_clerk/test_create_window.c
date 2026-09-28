#include "doc_clerk.h"
#include "klog.h"
#include "kstring.h"
#include "ledger.h"
#include "sched.h"
#include "shared.h"
#include <stdbool.h>

static int window_key = -1;

static int test_create_window(int width, int height, int x, int y) {
    gui_params_pack params;
    memset(&params, 0, sizeof(params));

    task_t *doc_clerk = task_get(doc_clerk_pid);

    if (doc_clerk == NULL) {
        ERROR("[DOC][CREATE_WINDOW]: Failed to fetch self. TEST FAILED\n");
        return STATUS_ERROR;
    }

    DEBUG_DOC("[DOC][CREATE_WINDOW]: Creating a window with properties: \n");
    DEBUG_DOC("[DOC][CREATE_WINDOW]: width: %d height: %d, x: %d, y:%d\n", width, height, x, y);

    params.opcode   = CREATE;
    params.width    = width;
    params.height   = height;
    params.x        = x;
    params.y        = y;
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

int doc_test_create_window(task_t *doc_clerk, request_table *req) {
    window_test_case_t tests[] = {
        {200, 200, 200, 200, STATUS_OK},
        {1200, 2200, 200, 200, STATUS_OK},
        {10, 20, 200, 200, STATUS_OK},
        {-200, 500, 20, 230, STATUS_OK},
        {200, 500, -20, -230, STATUS_OK}};

    int num_tests   = sizeof(tests) / sizeof(tests[0]);
    int test_failed = false;

    for (int i = 0; i < num_tests; i++) {
        window_test_case_t *t = &tests[i];

        if (test_create_window(t->width, t->height, t->x, t->y) != t->expected_status) {
            test_failed = true;
            break;
        }

        if (t->expected_status == STATUS_OK) {
            if (test_delete_window() == STATUS_ERROR) {
                test_failed = true;
                break;
            }
        }
    }

    if (test_failed) {
        req->status = FAILED;
        scheduler_wake_task(req->caller_pid);
        return STATUS_ERROR;
    }

    req->status         = COMPLETE;
    doc_clerk->priority = PRIORITY_NORMAL;
    scheduler_wake_task(req->caller_pid);
    return STATUS_OK;
}
