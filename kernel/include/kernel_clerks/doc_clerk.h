#ifndef DOC_CLERK_H
#define DOC_CLERK_H

#include "task.h"

typedef struct {
    int x;
    int y;
    int width;
    int height;
    int expected_status;
} window_test_case_t;

void doc_clerk_loop();
void doc_init(const task_t *doc_clerk);
int doc_test_core_sys();
int doc_test_create_window(task_t *doc_clerk);
int doc_test_resize_window(task_t *doc_clerk);
int doc_test_filesystem(task_t *doc_clerk);
#endif
