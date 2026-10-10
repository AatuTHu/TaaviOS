#include "doc_clerk.h"
#include "blankie.h"
#include "config.h"
#include "hail_mary.h"
#include "klog.h"
#include "kmalloc.h"
#include "ledger.h"
#include "paging.h"
#include "pmm.h"
#include "sched.h"
#include "shared.h"
#include "vmm.h"

#define TEST_BUFFER_SIZE 10

static void print_ledger_status() {
    int request_count = ledger_count_active_reqs();
    DEBUG_DOC("[DOC][PRINT_LEDGER_STATUS]: Current active requests %d\n", request_count);
    request_count = ledger_count_clerk_reqs(fs_task_pid);
    DEBUG_DOC("[DOC][PRINT_LEDGER_STATUS]: Fs request count %d\n", request_count);
    request_count = ledger_count_clerk_reqs(gui_task_pid);
    DEBUG_DOC("[DOC][PRINT_LEDGER_STATUS]: Gui request count %d\n", request_count);
    request_count = ledger_count_clerk_reqs(reaper_task_pid);
    DEBUG_DOC("[DOC][PRINT_LEDGER_STATUS]: Reaper request count %d\n", request_count);
    request_count = ledger_count_clerk_reqs(doc_clerk_pid);
    DEBUG_DOC("[DOC][PRINT_LEDGER_STATUS]: Doc request count %d\n", request_count);
    if (ledger_has_killable_reqs() > 0) {
        DEBUG_DOC("[DOC][PRINT_LEDGER_STATUS]: Ledger has killable requests %d\n", request_count);
    }

    int mem = pmm_get_free_pages();
    DEBUG_DOC("[DOC][PRINT_LEDGER_STATUS]: There are %d pysical pages free\n", mem);
    mem = pmm_get_used_pages();
    DEBUG_DOC("[DOC][PRINT_LEDGER_STATUS]: There are %d pysical pages in use\n", mem);
}

int doc_test_core_sys() {

    uint32_t physical_mm = pmm_alloc();

    if (physical_mm == 0) {
        DEBUG_DOC("[DOC][TEST_CORE]: Failed to allocate a physical memory page\n");
        return STATUS_ERROR;
    }
    DEBUG_DOC("[DOC][TEST_CORE]: ALLOCATING PHYSICAL MEMORY PASSED\n");
    if (pmm_free(physical_mm) == STATUS_ERROR) {
        DEBUG_DOC("[DOC][TEST_CORE]: Failed to free physical memory page\n");
        return STATUS_ERROR;
    }
    DEBUG_DOC("[DOC][TEST_CORE]: FREEING PHYSICAL MEMORY PASSED\n");
    page_directory_t *test_page_directory = paging_create_directory();

    if (test_page_directory == NULL) {
        DEBUG_DOC("[DOC][TEST_CORE]: Failed to create page directory\n");
        return STATUS_ERROR;
    }

    DEBUG_DOC("[DOC][TEST_CORE]: CREATING PAGE DIRECTORY PASSED\n");
    if (vmm_alloc(test_page_directory, TEST_ADDR, PAGE_SIZE, PAGE_USER_RW) == STATUS_ERROR) {
        DEBUG_DOC("[DOC][TEST_CORE]: Failed to allocate memory inside the test_page_directory\n");
        return STATUS_ERROR;
    }
    DEBUG_DOC("[DOC][TEST_CORE]: ALLOCATING MEMORY TO PAGE DIRECTORY PASSED\n");
    if (vmm_free_user_space(test_page_directory) == STATUS_ERROR) {
        DEBUG_DOC("[DOC][TEST_CORE]: Failed the virtual memory inside the test_page_directory\n");
        return STATUS_ERROR;
    }
    DEBUG_DOC("[DOC][TEST_CORE]: DESTROYING PAGE DIRECTORY PASSED\n");
    char *buf = (char *)kmalloc(TEST_BUFFER_SIZE);

    if (buf == NULL) {
        DEBUG_DOC("[DOC][TEST_CORE]: Failed to allocate heap memory the size of %d\n", TEST_BUFFER_SIZE);
        return STATUS_ERROR;
    }
    DEBUG_DOC("[DOC][TEST_CORE]: HEAP ALLOCATION PASSED\n");
    if (kfree(buf) == STATUS_ERROR) {
        DEBUG_DOC("[DOC][TEST_CORE]: Failed to free the heap memory buffer\n");
        return STATUS_ERROR;
    }
    DEBUG_DOC("[DOC][TEST_CORE]: HEAP FREE PASSED\n");
    return STATUS_OK;
}

static int doc_handle_req(request_table *req) {
    task_t *doc_clerk = task_get(doc_clerk_pid);

    if (doc_clerk == NULL) {
        ERROR("[DOC][HANDLE_REQ]: Could not find doc clerk. Aborting\n");
        return STATUS_ERROR;
    }

    switch (req->request_type) {
    case GUI_CLERK:
        doc_test_create_window(doc_clerk);
        doc_test_resize_window(doc_clerk);
        req->status         = COMPLETE;
        doc_clerk->priority = PRIORITY_NORMAL;
        scheduler_wake_task(req->caller_pid);
        break;
    case FS_CLERK:
        doc_test_filesystem(doc_clerk);
        req->status         = COMPLETE;
        doc_clerk->priority = PRIORITY_NORMAL;
        scheduler_wake_task(req->caller_pid);
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
    // DEBUG_DOC("[DOC][DOC_LOOP]\n");
    while (1) {
        print_ledger_status();

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
