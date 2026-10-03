#include "config.h"
#include "doc_clerk.h"
#include "klog.h"
#include "kstring.h"
#include "ledger.h"
#include "sched.h"
#include "shared.h"
#include "task.h"
#include <stdint.h>

static int make_request_fetch_results(task_t *doc_clerk, uint8_t opcode, int fd, char *buf, uint32_t buf_len, uint32_t flags) {

    ledger_add_fs_req(doc_clerk_pid, opcode, fd, buf, buf_len, flags);

    doc_clerk->state = TASK_BLOCKED;
    scheduler_yield(&doc_clerk->context);

    return ledger_collect(doc_clerk_pid, fs_task_pid, buf);
}

int doc_test_filesystem(task_t *doc_clerk) {

    if (make_request_fetch_results(doc_clerk, CREATE, 0, "DOC", strlen("DOC"), 0) == STATUS_ERROR) {
        ERROR("[DOC][TEST_FILE_SYSTEM]: Failed to create directory\n");
        return STATUS_ERROR;
    }

    DEBUG_DOC("[DOC][TEST_FILE_SYSTEM]: Succeeded in creating the directory\n");

    if (make_request_fetch_results(doc_clerk, FIND, 0, "/DOC", strlen("/DOC"), 0) == STATUS_ERROR) {
        ERROR("[DOC][TEST_FILE_SYSTEM]: Failed to move to the directory\n");
        return STATUS_ERROR;
    }

    DEBUG_DOC("[DOC][TEST_FILE_SYSTEM]: Succeeded in moving to the directory\n");

    if (make_request_fetch_results(doc_clerk, OPEN, 0, "DOC.TXT", strlen("DOC.TXT"), O_CREAT) STATUS_ERROR) {
        ERROR("[DOC][TEST_FILE_SYSTEM]: Failed to create the file\n");
        return STATUS_ERROR;
    }

    int fd = make_request_fetch_results(doc_clerk, OPEN, 0, "DOC.TXT", strlen("DOC.TXT"), O_RDWR);

    if (fd == STATUS_ERROR) {
        ERROR("[DOC][TEST_FILE_SYSTEM]: Failed to create file\n");
        return STATUS_ERROR;
    }

    DEBUG_DOC("[DOC][TEST_FILE_SYSTEM]: Succeeded in creating a file\n");

    if (make_request_fetch_results(doc_clerk, WRITE, fd, "DOC\0", strlen("DOC\0"), 0) == STATUS_ERROR) {
        ERROR("[DOC][TEST_FILE_SYSTEM]: Failed to write to the file\n");
        return STATUS_ERROR;
    }

    DEBUG_DOC("[DOC][TEST_FILE_SYSTEM]: Succeeded in writing to the file\n");

    char buf[512] = {0};
    if (make_request_fetch_results(doc_clerk, READ, fd, buf, 512, 0) == STATUS_ERROR) {
        ERROR("[DOC][TEST_FILE_SYSTEM]: Failed to read the file\n");
        return STATUS_ERROR;
    }

    DEBUG_DOC("[DOC][TEST_FILE_SYSTEM]: Succeeded in reading the file\n");

    if (make_request_fetch_results(doc_clerk, CLOSE, fd, NULL, 0, 0) == STATUS_ERROR) {
        ERROR("[DOC][TEST_FILE_SYSTEM]: Failed to close the file\n");
        return STATUS_ERROR;
    }

    DEBUG_DOC("[DOC][TEST_FILE_SYSTEM]: Succeeded in closing the file\n");

    if (make_request_fetch_results(doc_clerk, DELETE, 0, "DOC.TXT", strlen("DOC.TXT"), 0) == STATUS_ERROR) {
        ERROR("[DOC][TEST_FILE_SYSTEM]: Failed to delete the file\n");
        return STATUS_ERROR;
    }

    DEBUG_DOC("[DOC][TEST_FILE_SYSTEM]: Succeeded in deleting the file\n");

    if (make_request_fetch_results(doc_clerk, FIND, 0, "../", strlen("../"), 0) == STATUS_ERROR) {
        ERROR("[DOC][TEST_FILE_SYSTEM]: Failed to move back\n");
        return STATUS_ERROR;
    }

    DEBUG_DOC("[DOC][TEST_FILE_SYSTEM]: Succeeded in moving one directory level back\n");

    if (make_request_fetch_results(doc_clerk, DELETE, 0, "DOC", strlen("DOC"), 0) == STATUS_ERROR) {
        ERROR("[DOC][TEST_FILE_SYSTEM]: Failed to delete the directory\n");
        return STATUS_ERROR;
    }

    DEBUG_DOC("[DOC][TEST_FILE_SYSTEM]: Succeeded in deleting the doc directory\n");
    return STATUS_OK;
}
