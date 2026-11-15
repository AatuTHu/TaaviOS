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

static int signal_clerks_to_release_memory(struct registers *r, uint16_t caller_pid, uint16_t target_pid) {
    DEBUG_SYSCALL("[SIG_RELEASE_MEMORY]: Requesting fs_task release allocated memory\n");
    scheduler_set_task_state(TASK_BLOCKED);
    ledger_queue_free_req(caller_pid, fs_clerk_pid, target_pid);
    scheduler_yield(r);

    if (ledger_collect(caller_pid, fs_clerk_pid, NULL) == STATUS_ERROR) {
        ERROR("[SIG_RELEASE_MEMORY]: FS_TASK failed to release memory \n");
    }

    DEBUG_SYSCALL("[SIG_RELEASE_MEMORY]: Requesting gui_task to release allocated memory\n");
    scheduler_set_task_state(TASK_BLOCKED);
    ledger_queue_free_req(caller_pid, gui_clerk_pid, target_pid);
    scheduler_yield(r);
    if (ledger_collect(caller_pid, gui_clerk_pid, NULL) == STATUS_ERROR) {
        ERROR("[SIG_RELEASE_MEMORY]: GUI_TASK failed to release memory \n");
    }

    DEBUG_SYSCALL("[SIG_RELEASE_MEMORY]: Requesting reaper to kill the task\n");
    scheduler_set_task_state(TASK_BLOCKED);
    ledger_queue_free_req(caller_pid, reaper_clerk_pid, target_pid);
    scheduler_yield(r);
    if (ledger_collect(caller_pid, reaper_clerk_pid, NULL) == STATUS_ERROR) {
        ERROR("[SIG_RELEASE_MEMORY]: REAPER_TASK failed to release memory \n");
    }

    return STATUS_OK;
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

    signal_clerks_to_release_memory(r, current->pid, current->pid);
    scheduler_set_task_state(TASK_DEAD);

    return STATUS_OK;
}

/**
 * sys_read - read text to buffer.
 *
 * Description:
 * This function reads from keyboard or from a file based on the fd given to it.
 *
 * Return: STAUS_ERROR || STATUS_OK || BUFFER.
 */
static intptr_t sys_read(struct registers *r) {
    uintptr_t fd = 0, buf = 0, buffer_length = 0;
    collect_params(r, &fd, &buf, &buffer_length);

    if (buffer_length <= 0 && (char *)buf != NULL) {
        buffer_length = strlen((char *)buf);
    }

    const task_t *current = scheduler_get_current_task();

    if (current == NULL) {
        return STATUS_ERROR;
    }

    if (fd == 0) {
        int nread = 0;
        while ((nread = keyboard_read_from_buffer((char *)buf, current->pid)) == 0) {
            scheduler_set_task_state(TASK_BLOCKED);
            scheduler_yield(r);
        }
        return nread;
    }

    scheduler_set_task_state(TASK_BLOCKED);
    ledger_add_fs_req(current->pid, READ, fd, (char *)buf, buffer_length, O_RDONLY);
    scheduler_yield(r);

    if (ledger_collect(current->pid, fs_clerk_pid, (char *)buf) == STATUS_ERROR)
        return STATUS_ERROR;

    return STATUS_OK;
}

/**
 * sys_write - writes buffer given to it.
 *
 * Description:
 * This function writes contents of the buffer to either vga or to a file based on the fd number
 *
 * Return: STAUS_ERROR || STATUS_OK.
 */
static intptr_t sys_write(struct registers *r) {
    uintptr_t fd = 0, buf = 0, buffer_length = 0;
    collect_params(r, &fd, &buf, &buffer_length);

    if (buffer_length <= 0 && (char *)buf != NULL) {
        buffer_length = strlen((char *)buf);
    }

    const task_t *current = scheduler_get_current_task();

    if (current == NULL) {
        return STATUS_ERROR;
    }

    switch (fd) {
    case 1:
        vga_write((char *)buf);
        break;
    case 2:
        break;
    default:
        ledger_add_fs_req(current->pid, WRITE, fd, (char *)buf, buffer_length, O_WRONLY);
        scheduler_set_task_state(TASK_BLOCKED);
        scheduler_yield(r);
        return ledger_collect(current->pid, fs_clerk_pid, NULL);
    }

    return STATUS_OK;
}

/**
 * sys_open - opens a file.
 *
 * Description:
 *  Opens a file from the filesystem
 *
 * Return: STAUS_ERROR || STATUS_OK.
 */
static intptr_t sys_open(struct registers *r) {
    uintptr_t path = 0, path_length = 0, flags = 0;
    collect_params(r, &path, &path_length, &flags);

    const task_t *current = scheduler_get_current_task();

    if (current == NULL)
        return STATUS_ERROR;

    ledger_add_fs_req(current->pid, OPEN, 0, (char *)path, path_length, flags);
    scheduler_set_task_state(TASK_BLOCKED);
    scheduler_yield(r);

    return ledger_collect(current->pid, fs_clerk_pid, NULL);
}

/**
 * sys_close - closes a file.
 *
 * Description:
 * This function closes the opened file
 *
 * Return: STAUS_ERROR || STATUS_OK.
 */
static intptr_t sys_close(struct registers *r) {
    DEBUG_SYSCALL("[SYSCALL][SYS_CLOSE]\n");
    uintptr_t fd = 0;
    collect_params(r, &fd, NULL, NULL);

    if (fd <= 2 || fd > MAX_FD_ENTRIES) {
        ERROR("[SYSCALL][SYS_CLOSE]: fd %d provided was invalid\n", fd);
        return STATUS_ERROR;
    }

    ledger_add_fs_req(this_core()->sched.current_pid, CLOSE, fd, NULL, 0, 0);
    scheduler_set_task_state(TASK_BLOCKED);
    scheduler_yield(r);

    return ledger_collect(this_core()->sched.current_pid, fs_clerk_pid, NULL);
}

static intptr_t sys_unlink(struct registers *r) {
    DEBUG_SYSCALL("[SYSCALL][SYS_UNLINK]\n");
    uintptr_t path = 0;
    collect_params(r, &path, NULL, NULL);

    ledger_add_fs_req(this_core()->sched.current_pid, DELETE, 0, (char *)path, strlen((char *)path), 0);
    scheduler_set_task_state(TASK_BLOCKED);
    scheduler_yield(r);

    return ledger_collect(this_core()->sched.current_pid, fs_clerk_pid, NULL);
}

static intptr_t sys_getpid(struct registers *r) {
    (void)r;
    const task_t *task = scheduler_get_current_task();
    return (task) ? (intptr_t)task->pid : -1;
}

/**
 * sys_chdir - changes working directory
 *
 * Description:
 * This function closes the opened file
 *
 * Return: STAUS_ERROR || opened directory.
 */
static intptr_t sys_chdir(struct registers *r) {
    uintptr_t path = 0, path_length = 0;
    collect_params(r, &path, &path_length, NULL);

    if (path_length <= 0 && (char *)path != NULL) {
        path_length = strlen((char *)path);
    }

    ledger_add_fs_req(this_core()->sched.current_pid, FIND, 0, (char *)path, path_length, 0);
    scheduler_set_task_state(TASK_BLOCKED);
    scheduler_yield(r);

    return ledger_collect(this_core()->sched.current_pid, fs_clerk_pid, NULL);
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

/**
 * sys_mkdir - create directory.
 *
 * Description:
 * Creates a new folder.
 *
 * Return: STAUS_ERROR || STATUS_OK.
 */
static intptr_t sys_mkdir(struct registers *r) {
    uintptr_t path = 0, path_length = 0;
    collect_params(r, &path, &path_length, NULL);

    if (path_length <= 0 && (char *)path != NULL) {
        path_length = strlen((char *)path);
    }

    ledger_add_fs_req(this_core()->sched.current_pid, CREATE, 0, (char *)path, path_length, O_CREAT);
    scheduler_set_task_state(TASK_BLOCKED);
    scheduler_yield(r);

    return ledger_collect(this_core()->sched.current_pid, fs_clerk_pid, NULL);
}

static intptr_t sys_getdents(struct registers *r) {
    uintptr_t buffer = 0, buffer_length = 0;
    collect_params(r, &buffer, &buffer_length, NULL);

    if (buffer_length <= 0 && (char *)buffer != NULL) {
        buffer_length = strlen((char *)buffer);
    }

    ledger_add_fs_req(this_core()->sched.current_pid, LIST, 0,
                      (char *)buffer, buffer_length, O_RDONLY);
    scheduler_set_task_state(TASK_BLOCKED);
    scheduler_yield(r);

    return ledger_collect(this_core()->sched.current_pid, fs_clerk_pid, (char *)buffer);
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

    task_t *target_task = task_get(target_pid);

    if (target_task == NULL || target_task->pid < CLERK_COUNT) {
        return STATUS_ERROR;
    }

    if (this_core()->sched.current_pid == (uint16_t)target_task->pid) {
        sys_exit(r);
    }

    target_task->state = TASK_DEAD;
    return signal_clerks_to_release_memory(r, this_core()->sched.current_pid, target_task->pid);
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
    default:
        break;
    }

    return STATUS_ERROR;
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
    syscall_table[SYS_EXIT]     = sys_exit;
    syscall_table[SYS_WRITE]    = sys_write;
    syscall_table[SYS_GETPID]   = sys_getpid;
    syscall_table[SYS_EXEC]     = sys_exec;
    syscall_table[SYS_READ]     = sys_read;
    syscall_table[SYS_IDLE]     = sys_idle;
    syscall_table[SYS_YIELD]    = sys_yield;
    syscall_table[SYS_OPEN]     = sys_open;
    syscall_table[SYS_CLOSE]    = sys_close;
    syscall_table[SYS_KILL]     = sys_kill;
    syscall_table[SYS_MKDIR]    = sys_mkdir;
    syscall_table[SYS_CHDIR]    = sys_chdir;
    syscall_table[SYS_GETDENTS] = sys_getdents;
    syscall_table[SYS_WI]       = sys_window;
    syscall_table[SYS_IOCTL]    = sys_ioctl;
    syscall_table[SYS_SBRK]     = sys_sbrk;
    syscall_table[SYS_UNLINK]   = sys_unlink;
}
