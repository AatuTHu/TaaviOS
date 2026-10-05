#include "config.h"
#include "doc_clerk.h"
#include "klog.h"
#include "kstring.h"
#include "ledger.h"
#include "sched.h"
#include "shared.h"
#include "task.h"
#include <stdbool.h>

static int window_key = -1;

static int __resize_window(task_t *doc_clerk, int width, int height) {
    gui_params_pack params;
    memset(&params, 0, sizeof(params));

    params.opcode     = RESIZE;
    params.struct_key = window_key;
    params.width      = width;
    params.height     = height;
    params.fg_color   = COLOR_LIGHT_GRAY;
    params.bg_color   = COLOR_BLACK;

    if (ledger_add_gui_req(doc_clerk_pid, &params) == STATUS_ERROR) {
        ERROR("[DOC][RESIZE_WINDOW]: Setting up the test failed\n");
        return STATUS_ERROR;
    }
    doc_clerk->state = TASK_BLOCKED;
    scheduler_yield(&doc_clerk->context);

    return ledger_collect(doc_clerk_pid, gui_task_pid, NULL);
}

static int __create_window(task_t *doc_clerk, int width, int height, int x, int y) {
    gui_params_pack params;
    memset(&params, 0, sizeof(params));

    params.opcode   = CREATE;
    params.width    = width;
    params.height   = height;
    params.x        = x;
    params.y        = y;
    params.fg_color = COLOR_LIGHT_GRAY;
    params.bg_color = COLOR_BLACK;

    if (ledger_add_gui_req(doc_clerk_pid, &params) == STATUS_ERROR) {
        ERROR("[DOC][CREATE_WINDOW]: Setting up the test failed\n");
        return STATUS_ERROR;
    }
    doc_clerk->state = TASK_BLOCKED;
    scheduler_yield(&doc_clerk->context);

    window_key = ledger_collect(doc_clerk_pid, gui_task_pid, NULL);

    return window_key == STATUS_ERROR ? STATUS_ERROR : STATUS_OK;
}

static int __delete_window(task_t *doc_clerk) {

    doc_clerk->state = TASK_BLOCKED;
    if (ledger_queue_free_req(doc_clerk_pid, gui_task_pid, doc_clerk_pid) == STATUS_ERROR) {
        ERROR("[DOC][DELETE_WINDOW]: queue failed\n");
        return STATUS_ERROR;
    }
    scheduler_yield(&doc_clerk->context);

    int delete_result = ledger_collect(doc_clerk_pid, gui_task_pid, NULL);

    return delete_result == STATUS_ERROR ? STATUS_ERROR : STATUS_OK;
}

int doc_test_create_window(task_t *doc_clerk) {

    window_test_case_t tests[] = {
        {200, 200, 200, 200, STATUS_OK},
        {1200, 2200, 200, 200, STATUS_ERROR},
        {10, 20, 200, 200, STATUS_OK},
        {-200, 500, 20, 230, STATUS_ERROR},
        {200, 500, -20, -230, STATUS_ERROR},
    };

    int num_tests = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < num_tests; i++) {
        window_test_case_t *t = &tests[i];

        if (__create_window(doc_clerk, t->width, t->height, t->x, t->y) != t->expected_status) {
            ERROR("[DOC][CREATE_WINDOW]: TEST %d FAILED\n", i);
            continue;
        }

        DEBUG_DOC("[DOC][CREATE_WINDOW]: TEST %d PASSED\n", i);
        if (t->expected_status == STATUS_OK) {
            if (__delete_window(doc_clerk) == STATUS_ERROR) {
                ERROR("[DOC][DELETE_WINDOW]: Failed to DELETE %d window\n", i);
                break;
            }
            window_key = -1;
        }
    }

    return STATUS_OK;
}

int doc_test_resize_window(task_t *doc_clerk) {
    window_test_case_t tests[] = {
        {10, 10, 100, 100, STATUS_OK},
        {10, 10, 600, 1000, STATUS_OK},
        {10, 10, -200, 800, STATUS_ERROR},
        {10, 10, 1324, 500, STATUS_ERROR},
        {10, 10, 10, 10, STATUS_OK},
    };

    int num_tests = sizeof(tests) / sizeof(tests[0]);

    if (__create_window(doc_clerk, 400, 400, 10, 10) == STATUS_ERROR) {
        ERROR("[DOC][RESIZE_WINDOW]: Setting up window failed\n");
        return STATUS_ERROR;
    }

    for (int i = 0; i < num_tests; i++) {
        window_test_case_t *t = &tests[i];
        if (__resize_window(doc_clerk, t->width, t->height) != t->expected_status) {
            ERROR("[DOC][RESIZE_WINDOW]: TEST %d FAILED\n", i);
            continue;
        }
        DEBUG_DOC("[DOC][RESIZE_WINDOW]: TEST %d PASSED\n", i);
    }

    if (__delete_window(doc_clerk) == STATUS_ERROR) {
        ERROR("[DOC][DELETE_WINDOW]: Failed to DELETE window\n");
        return STATUS_ERROR;
    }
    window_key = -1;

    return STATUS_OK;
}
