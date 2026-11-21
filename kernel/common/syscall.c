#include "syscall.h"
#include "config.h"
#include "cpu.h"
#include "elf.h"
#include "fat32.h"
#include "keyboard.h"
#include "klog.h"
#include "ledger.h"
#include "paging.h"
#include "sched.h"
#include "shared.h"
#include "task.h"
#include "vga.h"
#include "vmm.h"
#include <stdbool.h>
#include <stdint.h>

static syscall_fn_t syscall_table[MAX_SYSCALLS];

//
// Sufficient
//
static inline void collect_params(struct registers *r, uintptr_t *p1, uintptr_t *p2, uintptr_t *p3) {
#if defined(__x86_64__)
    if (p1)
        *p1 = r->rbx;
    if (p2)
        *p2 = r->rcx;
    if (p3)
        *p3 = r->rdx;
#else
    if (p1)
        *p1 = r->ebx;
    if (p2)
        *p2 = r->ecx;
    if (p3)
        *p3 = r->edx;
#endif
}

static int signal_clerks_to_release_memory(uint16_t caller_pid, uint16_t target_pid) {
    DEBUG_SYSCALL("[SIG_RELEASE_MEMORY]: Requesting fs_task release allocated memory\n");
    ledger_queue_free_req(caller_pid, fs_clerk_pid, target_pid);

    DEBUG_SYSCALL("[SIG_RELEASE_MEMORY]: Requesting gui_task to release allocated memory\n");
    ledger_queue_free_req(caller_pid, gui_clerk_pid, target_pid);

    DEBUG_SYSCALL("[SIG_RELEASE_MEMORY]: Requesting reaper to kill the task\n");
    ledger_queue_free_req(caller_pid, reaper_clerk_pid, target_pid);

    return STATUS_OK;
}

/**
 - change_keyboard_focus - when user task wishes to switch to another task.
 - Description:
 - This functions routes the keyboard to be directed to window that the user wants to be "active"
 - creating the illusion of switching active windows. But in reality all windows are active and only the
 - tasks that gets keyboard access if switched
 - Context: Function was made so I could change between operator task and other tasks
 - Return: status_ok || status_error.
 */
static int change_keyboard_focus(uint16_t target_pid) {

    if (!(target_pid >= CLERK_COUNT || target_pid >= MAX_TASKS) || task_get(target_pid) == NULL) {
        return STATUS_ERROR;
    }

    if (keyboard_replace_cur_foreground_pid(target_pid) == STATUS_ERROR) {
        return STATUS_ERROR;
    }
    scheduler_set_task_state(TASK_BLOCKED);
    scheduler_wake_task(target_pid);

    return STATUS_OK;
}

/**
 * scan_kb_buffer - scans chars from kb buffer to given buffer.
 *
 *
 * Return: STAUS_ERROR || BUFFER.
 */
static intptr_t scan_kb_buffer(struct registers *r, char *buf) {

    int nread = 0;
    while ((nread = keyboard_read_from_buffer((char *)buf, this_core()->sched.current_pid)) == 0) {
        scheduler_set_task_state(TASK_BLOCKED);
        scheduler_yield(r);
    }
    return nread;
}

/**
 * sys_exit - marks a task dead.
 *
 * Description:
 * This function releases keyboard focus if it has it. Signals clerks to release reserved memory
 * related to the task. Then it marks the task as dead so scheduler cant pick it. After that it
 * yields tasks processor time
 *
 * Return: STAUS_ERROR || STATUS_OK.
 */
static intptr_t sys_exit(struct registers *r) {
    DEBUG_SYSCALL("[SYSCALL][EXIT]\n");
    const task_t *current = scheduler_get_current_task();

    if (current == NULL || current->pid < CLERK_COUNT) {
        return STATUS_ERROR;
    }

    if (current->state == TASK_DEAD) {
        return STATUS_OK;
    }

    if (keyboard_get_foreground_pid() == (int)current->pid) {
        keyboard_set_foreground_pid(-1);
    }

    signal_clerks_to_release_memory(current->pid, current->pid);
    scheduler_set_task_state(TASK_DEAD);
    scheduler_yield(r);
    return STATUS_OK;
}

static intptr_t sys_write(struct registers *r) {
    uintptr_t opcode = 0, buf = 0;
    collect_params(r, &opcode, &buf, NULL);

    switch (opcode) {
    case 1:
        vga_write((char *)buf);
        break;
    case 2:
        DEBUG((char *)buf);
        break;
    default:
        break;
    }

    return STATUS_OK;
}

static intptr_t sys_getpid(struct registers *r) {
    (void)r;
    return this_core()->sched.current_pid;
}

static intptr_t sys_sbrk(struct registers *r) {
    DEBUG_SYSCALL("[SYSCALL][SYS_SBRK]\n");

    task_t *current = scheduler_get_current_task();
    if (current == NULL) {
        ERROR("[SYSCALL][SYS_SBRK]: Could not find current task\n");
        return 0;
    }

    uintptr_t alloc_size = 0;
    collect_params(r, &alloc_size, NULL, NULL);

    if (alloc_size == 0) {
        return current->heap_end;
    }

    uintptr_t old_end      = current->heap_end;
    uintptr_t aligned_size = (alloc_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    if (vmm_alloc(current->page_dir, current->heap_end, aligned_size, PAGE_USER_RW) == STATUS_ERROR) {
        ERROR("[SYSCALL][SYS_SBRK]: Allocating more heap failed\n");
        return 0;
    }

    current->heap_end = old_end + aligned_size;
    return old_end;
}

/**
 * sys_exec - executes elf binaries.
 *
 * Description:
 * This function find the binary at the end of the fiven path.
 * then reads the file in to a buffer and makes a task of the binary.
 *
 * Return: STAUS_ERROR || STATUS_OK.
 */
static intptr_t sys_exec(struct registers *r) {
    DEBUG_SYSCALL("[SYSCALL][SYS_EXEC]\n");
    uintptr_t filename = 0;

    collect_params(r, NULL, &filename, NULL);

    uint32_t file_cluster = 0, dir_cluster = 0, file_size = 0;
    uint32_t start_dir_cluster = f32_fs.root_cluster;
    uint8_t file_attr          = 0;
    char task_name[TASK_NAME_LENGTH];

    if (fat32_find_cluster(start_dir_cluster, (char *)filename, &file_cluster,
                           &dir_cluster, &file_size, task_name, &file_attr) == STATUS_ERROR) {
        return STATUS_ERROR;
    }

    uint8_t *binary_buffer = (uint8_t *)kmalloc(file_size);
    if (binary_buffer == NULL) {
        return STATUS_ERROR;
    }

    if (fat32_read_file(file_cluster, file_size, binary_buffer) == STATUS_ERROR) {
        kfree(binary_buffer);
        return STATUS_ERROR;
    }

    page_directory_t *pd = vmm_create_directory();
    if (pd == NULL) {
        kfree(binary_buffer);
        return STATUS_ERROR;
    }

    uintptr_t elf_calculated_heap_start = 0;
    uintptr_t entry                     = elf_load((void *)binary_buffer, pd, &elf_calculated_heap_start);

    if (entry == 0) {
        vmm_free_user_space(pd);
        kfree(binary_buffer);
        return STATUS_ERROR;
    }

    kfree(binary_buffer);

    task_t *task = task_create(-1, entry, elf_calculated_heap_start, task_name, pd, USER_TASK);
    if (task == NULL || scheduler_add(task) == STATUS_ERROR) {
        vmm_free_user_space(pd);
        return STATUS_ERROR;
    }

    return STATUS_OK;
}

static intptr_t sys_idle(struct registers *r) {
    (void)r;
    while (1) __asm__ __volatile__("sti; hlt");
    return STATUS_OK;
}

static intptr_t sys_yield(struct registers *r) {
    scheduler_yield(r);
    return STATUS_OK;
}

static intptr_t sys_kill(struct registers *r) {
    uintptr_t target_pid = 0;
    collect_params(r, &target_pid, NULL, NULL);

    if (target_pid < CLERK_COUNT) {
        ERROR("[SYSCALL][SYS_KILL]: Cannot kill anything below %d\n", CLERK_COUNT - 1);
        return STATUS_ERROR;
    }

    if (this_core()->sched.current_pid == (uint16_t)target_pid) {
        sys_exit(r);
        return STATUS_OK;
    }

    task_t *target_task = task_get(target_pid);

    if (target_task == NULL || target_task->pid < CLERK_COUNT) {
        return STATUS_ERROR;
    }

    target_task->state = TASK_DEAD;
    return signal_clerks_to_release_memory(this_core()->sched.current_pid, target_task->pid);
}

static intptr_t sys_ioctl(struct registers *r) {
    uintptr_t opcode = 0, value = 0; // value for could be a sub opcode for ledger or a pid for kbd;
    collect_params(r, &opcode, &value, NULL);

    switch (opcode) {
    case SET_OPERATOR:
        return keyboard_set_operator_pid(this_core()->sched.current_pid);
    case CH_ACT_W:
        return change_keyboard_focus(value);
    case TEST: {
        if (ledger_add_doc_req(this_core()->sched.current_pid, value) == STATUS_ERROR) {
            break;
        }
        scheduler_set_task_state(TASK_BLOCKED);
        scheduler_yield(r);
        return ledger_collect(this_core()->sched.current_pid, doc_clerk_pid, NULL);
    }
    case READ:
        return scan_kb_buffer(r, (char *)value);
    default:
        break;
    }

    return STATUS_ERROR;
}

static intptr_t sys_filesystem(struct registers *r) {
#if defined(__x86_64__)
    fs_params_pack *params = (fs_params_pack *)r->rbx;
#else
    fs_params_pack *params = (fs_params_pack *)r->ebx;
#endif

    if (ledger_add_fs_req(this_core()->sched.current_pid, params) == STATUS_ERROR) {
        return STATUS_ERROR;
    }

    if (params->wfr == false) {
        return STATUS_OK;
    }

    scheduler_yield(r);
    return ledger_collect(this_core()->sched.current_pid, fs_clerk_pid, (char *)params->buf);
}

void syscall_dispatch(struct registers *r) {
#if defined(__x86_64__)
    if (r->rax >= MAX_SYSCALLS || syscall_table[r->rax] == NULL) {
        r->rax = STATUS_ERROR;
        return;
    }
    r->rax = syscall_table[r->rax](r);
#else
    if (r->eax >= MAX_SYSCALLS || syscall_table[r->eax] == NULL) {
        r->eax = STATUS_ERROR;
        return;
    }
    r->eax = syscall_table[r->eax](r);
#endif
}

void syscall_init() {
    for (uint32_t i = 0; i < MAX_SYSCALLS; i++) syscall_table[i] = NULL;
    syscall_table[SYS_EXIT]   = sys_exit;
    syscall_table[SYS_WRITE]  = sys_write;
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_EXEC]   = sys_exec;
    // syscall_table[SYS_READ]   = sys_read;
    syscall_table[SYS_IDLE]   = sys_idle;
    syscall_table[SYS_YIELD]  = sys_yield;
    syscall_table[SYS_KILL]   = sys_kill;
    syscall_table[SYS_WI]     = sys_window;
    syscall_table[SYS_FS]     = sys_filesystem;
    syscall_table[SYS_IOCTL]  = sys_ioctl;
    syscall_table[SYS_SBRK]   = sys_sbrk;
}
