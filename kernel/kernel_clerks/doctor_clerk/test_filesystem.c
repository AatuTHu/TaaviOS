#include "config.h"
#include "doc_clerk.h"
#include "klog.h"
#include "kmalloc.h"
#include "kstring.h"
#include "ledger.h"
#include "sched.h"
#include "shared.h"
#include "task.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * doc_clerk
 * Design & Implementation:
 * @author: A.H, 2026
 */

static int make_request_fetch_results(task_t *doc_clerk, uint8_t opcode, int fd, char *buf, uint32_t buf_len, uint32_t flags, bool wfr) {

    fs_params_pack *params = (fs_params_pack *)kmalloc(sizeof(fs_params_pack));

    if (params == NULL) {
        return STATUS_ERROR;
    }

    params->fd          = fd;
    params->opcode      = opcode;
    params->buf         = buf;
    params->buffer_size = buf_len;
    params->flags       = flags;
    params->wfr         = wfr;

    if (ledger_add_fs_req(doc_clerk_pid, params) == STATUS_ERROR) {
        return STATUS_ERROR;
    }

    if (!params->wfr) {
        return STATUS_OK;
    }

    scheduler_yield(&doc_clerk->context);
    return ledger_collect(doc_clerk_pid, fs_clerk_pid, buf);
}

int doc_test_filesystem(task_t *doc_clerk) {

    if (make_request_fetch_results(doc_clerk, CREATE, 0, "DOC", strlen("DOC"), 0, true) == STATUS_ERROR) {
        ERROR("[DOC][TEST_FILE_SYSTEM]: Failed to create directory\n");
        return STATUS_ERROR;
    }

    DEBUG_DOC("[DOC][TEST_FILE_SYSTEM]: Succeeded in creating a directory called 'DOC'\n");

    if (make_request_fetch_results(doc_clerk, FIND, 0, "/DOC", strlen("/DOC"), 0, true) == STATUS_ERROR) {
        ERROR("[DOC][TEST_FILE_SYSTEM]: Failed to move to the directory\n");
        return STATUS_ERROR;
    }

    DEBUG_DOC("[DOC][TEST_FILE_SYSTEM]: Succeeded in moving to the directory\n");

    if (make_request_fetch_results(doc_clerk, OPEN, 0, "DOC.TXT", strlen("DOC.TXT"), O_CREAT, true) == STATUS_ERROR) {
        ERROR("[DOC][TEST_FILE_SYSTEM]: Failed to create the file\n");
        return STATUS_ERROR;
    }
    DEBUG_DOC("[DOC][TEST_FILE_SYSTEM]: Succeeded in creating a file 'DOC.TXT'\n");

    int fd = make_request_fetch_results(doc_clerk, OPEN, 0, "DOC.TXT", strlen("DOC.TXT"), O_RDWR, true);

    if (fd == STATUS_ERROR) {
        ERROR("[DOC][TEST_FILE_SYSTEM]: Failed to create file\n");
        return STATUS_ERROR;
    }
    DEBUG_DOC("[DOC][TEST_FILE_SYSTEM]: Succeeded in opening the file in RDWR mode\n");

    if (make_request_fetch_results(doc_clerk, WRITE, fd, "DOC\0", strlen("DOC\0"), 0, true) == STATUS_ERROR) {
        ERROR("[DOC][TEST_FILE_SYSTEM]: Failed to write to the file\n");
        return STATUS_ERROR;
    }

    DEBUG_DOC("[DOC][TEST_FILE_SYSTEM]: Succeeded in writing to the file\n");

    char buf[512] = {0};
    if (make_request_fetch_results(doc_clerk, READ, fd, buf, 512, 0, true) == STATUS_ERROR) {
        ERROR("[DOC][TEST_FILE_SYSTEM]: Failed to read the file\n");
        return STATUS_ERROR;
    }

    DEBUG_DOC("[DOC][TEST_FILE_SYSTEM]: Succeeded in reading the file\n");

    if (make_request_fetch_results(doc_clerk, CLOSE, fd, NULL, 0, 0, false) == STATUS_ERROR) {
        ERROR("[DOC][TEST_FILE_SYSTEM]: Failed to close the file\n");
        return STATUS_ERROR;
    }

    DEBUG_DOC("[DOC][TEST_FILE_SYSTEM]: Succeeded in closing the file\n");

    if (make_request_fetch_results(doc_clerk, DELETE, 0, "DOC.TXT", strlen("DOC.TXT"), 0, true) == STATUS_ERROR) {
        ERROR("[DOC][TEST_FILE_SYSTEM]: Failed to delete the file\n");
        return STATUS_ERROR;
    }

    DEBUG_DOC("[DOC][TEST_FILE_SYSTEM]: Succeeded in deleting the file\n");

    if (make_request_fetch_results(doc_clerk, FIND, 0, "../", strlen("../"), 0, true) == STATUS_ERROR) {
        ERROR("[DOC][TEST_FILE_SYSTEM]: Failed to move back\n");
        return STATUS_ERROR;
    }

    DEBUG_DOC("[DOC][TEST_FILE_SYSTEM]: Succeeded in moving one directory level back\n");

    if (make_request_fetch_results(doc_clerk, DELETE, 0, "DOC", strlen("DOC"), 0, true) == STATUS_ERROR) {
        ERROR("[DOC][TEST_FILE_SYSTEM]: Failed to delete the directory\n");
        return STATUS_ERROR;
    }

    ledger_add_general_req(doc_clerk_pid, fs_clerk_pid, doc_clerk_pid, FREE, false);
    DEBUG_DOC("[DOC][TEST_FILE_SYSTEM]: Succeeded in deleting the doc directory\n");
    return STATUS_OK;
}
