#include "syscall.h"
#include "config.h"
#include "elf.h"
#include "fat32.h"
#include "keyboard.h"
#include "klog.h"
#include "ledger.h"
#include "sched.h"
#include "shared.h"
#include "task.h"
#include "vga.h"
#include "vmm.h"
#include <stdint.h>

static syscall_fn_t syscall_table[MAX_SYSCALLS];

/**
 * sys_exit - marks a task dead.
 *
 * Description:
 * This function releases it's hold on keyboard and marks itself as dead. Then
 * yields
 *
 * Return: STAUS_ERROR || STATUS_OK.
 */
static int32_t sys_exit(struct registers *r) {
    const task_t *current = scheduler_get_current_task();

    if (current == NULL) {
        return STATUS_ERROR;
    }

    if (!(current->pid >= CLERK_COUNT)) {
        return STATUS_ERROR;
    }

    if (current->state == TASK_DEAD) {
        return STATUS_OK;
    }

    if (keyboard_get_foreground_pid() == (int)current->pid) {
        keyboard_set_foreground_pid(-1);
    }

    DEBUG_SYSCALL("[SYSCALL][SYS_EXIT]: Requesting fs_task release allocated memory\n");
    scheduler_set_task_state(TASK_BLOCKED);
    ledger_add_fs_free_req(current->pid, current->pid);
    scheduler_yield(r);
    ledger_collect(current->pid, fs_task_pid, NULL);

    DEBUG_SYSCALL("[SYSCALL][SYS_EXIT]: Requesting gui_task to release allocated memory\n");
    scheduler_set_task_state(TASK_BLOCKED);
    ledger_add_gui_free_req(current->pid, current->pid);
    scheduler_yield(r);
    ledger_collect(current->pid, gui_task_pid, NULL);

    DEBUG_SYSCALL("[SYSCALL][SYS_EXIT]: Requesting reaper to kill the task\n");
    scheduler_set_task_state(TASK_BLOCKED);
    ledger_add_reaper_req(current->pid, current->pid);
    scheduler_yield(r);
    ledger_collect(current->pid, reaper_task_pid, NULL);

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
static int32_t sys_read(struct registers *r) {
    int fd                = r->ebx;
    char *buf             = (char *)r->ecx;
    uint32_t buff_size    = (uint32_t)r->edx;
    const task_t *current = scheduler_get_current_task();

    if (current == NULL) {
        return STATUS_ERROR;
    }

    if (fd == 0) {
        int nread = 0;
        while ((nread = keyboard_read_from_buffer(buf, current->pid)) == 0) {
            scheduler_set_task_state(TASK_BLOCKED);
            scheduler_yield(r);
        }
        return nread;
    }

    ledger_add_fs_req(current->pid, READ, fd, NULL, NULL, buff_size, O_RDONLY);
    scheduler_set_task_state(TASK_BLOCKED);
    scheduler_yield(r);

    if (ledger_collect(current->pid, fs_task_pid, buf) == STATUS_ERROR)
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
static int32_t sys_write(struct registers *r) {
    int fd                = r->ebx;
    const char *buf       = (char *)r->ecx;
    uint32_t len          = r->edx;
    const task_t *current = scheduler_get_current_task();

    if (current == NULL) {
        return STATUS_ERROR;
    }

    switch (fd) {
    case 1:
        ledger_add_gui_text(current->pid, WRITE, 0, 0, len, buf);
        scheduler_set_task_state(TASK_BLOCKED);
        scheduler_yield(r);
        return ledger_collect(current->pid, gui_task_pid, NULL);
    case 2:
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        ERROR(buf);
        r->eax = STATUS_OK;
        break;

    default:
        ledger_add_fs_req(current->pid, WRITE, fd, NULL, buf, len, O_WRONLY);
        scheduler_set_task_state(TASK_BLOCKED);
        scheduler_yield(r);
        return ledger_collect(current->pid, fs_task_pid, NULL);
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
static int32_t sys_open(struct registers *r) {
    const char *path      = (char *)r->ebx;
    uint32_t flags        = r->ecx;
    const task_t *current = scheduler_get_current_task();

    if (current == NULL)
        return STATUS_ERROR;

    ledger_add_fs_req(current->pid, OPEN, 0, path, NULL, 0, flags);
    scheduler_set_task_state(TASK_BLOCKED);
    scheduler_yield(r);

    return ledger_collect(current->pid, fs_task_pid, NULL);
}

/**
 * sys_close - closes a file.
 *
 * Description:
 * This function closes the opened file
 *
 * Return: STAUS_ERROR || STATUS_OK.
 */
static int32_t sys_close(struct registers *r) {
    DEBUG_SYSCALL("[SYSCALL][SYS_CLOSE]\n");
    int fd                = r->ebx;
    const task_t *current = scheduler_get_current_task();

    if (fd <= 2 || fd > MAX_FD_ENTRIES || current == NULL) {
        ERROR("[SYSCALL][SYS_CLOSE]: fd %d provided was invalid\n", fd);
        return STATUS_ERROR;
    }

    ledger_add_fs_req(current->pid, CLOSE, fd, NULL, NULL, 0, 0);
    scheduler_set_task_state(TASK_BLOCKED);
    scheduler_yield(r);

    return ledger_collect(current->pid, fs_task_pid, NULL);
}

static int32_t sys_getpid(struct registers *r) {
    (void)r;
    const task_t *task = scheduler_get_current_task();
    return (task) ? (int32_t)task->pid : -1;
}

/**
 * sys_chdir - changes working directory
 *
 * Description:
 * This function closes the opened file
 *
 * Return: STAUS_ERROR || opened directory.
 */
static int32_t sys_chdir(struct registers *r) {
    const char *path      = (char *)r->ebx;
    uint32_t len          = r->ecx;
    const task_t *current = scheduler_get_current_task();

    if (current == NULL) {
        return STATUS_ERROR;
    }

    ledger_add_fs_req(current->pid, FIND, 0, path, NULL, len, 0);
    scheduler_set_task_state(TASK_BLOCKED);
    scheduler_yield(r);

    return ledger_collect(current->pid, fs_task_pid, NULL);
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
static int32_t sys_exec(struct registers *r) {
    DEBUG_SYSCALL("[SYSCALL][SYS_EXEC]\n");
    task_t *current      = scheduler_get_current_task();
    const char *filename = (char *)r->ecx;

    if (current == NULL || filename == NULL) {
        return STATUS_ERROR;
    }

    uint32_t file_cluster      = 0;
    uint32_t dir_cluster       = 0;
    uint32_t file_size         = 0;
    uint32_t start_dir_cluster = f32_fs.root_cluster;
    uint8_t file_attr          = 0;
    char task_name[TASK_NAME_LENGTH];

    DEBUG_SYSCALL("[SYSCALL][SYS_EXEC]: Trying to find %s\n", filename);
    if (fat32_find_cluster(start_dir_cluster, filename, &file_cluster, &dir_cluster, &file_size, task_name, &file_attr) ==
        STATUS_ERROR) {
        ERROR("[SYS_EXEC]: Did not find the file\n");
        return STATUS_ERROR;
    }

    DEBUG_SYSCALL("[SYSCALL][SYS_EXEC]: continuing to read file with name %s\n", task_name);

    uint8_t *binary_buffer = (uint8_t *)kmalloc(file_size);

    if (binary_buffer == NULL) {
        ERROR("[SYSCALL][SYS_EXEC]: Was not able to allocate buffer for file read\n");
        return STATUS_ERROR;
    }

    if (fat32_read_file(file_cluster, file_size, binary_buffer) == STATUS_ERROR) {
        ERROR("[SYS_EXEC]: Could not read the file\n");
        kfree(binary_buffer);
        return STATUS_ERROR;
    }

    DEBUG_SYSCALL("[SYSCALL][SYS_EXEC]: creating the task\n");

    page_directory_t *pd = vmm_create_directory();

    if (pd == NULL) {
        ERROR("[SYSCALL][SYS_EXEC]: Invalid page directory. Aborting\n");
        kfree(binary_buffer);
        return STATUS_ERROR;
    }

    int entry = elf_load(binary_buffer, pd);

    if (entry == STATUS_ERROR) {
        ERROR("[SYSCALL][SYS_EXEC]: elf load failed aborting.\n");
        vmm_free_user_space(pd);
        goto failure;
    }

    kfree(binary_buffer);

    task_t *task = task_create(-1, entry, task_name, pd, USER_TASK);

    if (task == NULL) {
        goto failure;
    }

    if (scheduler_add(task) == STATUS_ERROR) {
        ERROR("[SYSCALL][SYS_EXEC]: Failed to add task to scheduler\n");
        goto failure;
    }

    DEBUG_SYSCALL("[SYSCALL][SYS_EXEC]: Task created %d\n", task->pid);
    return STATUS_OK;

failure:
    vmm_free_user_space(pd);
    kfree(binary_buffer);
    return STATUS_ERROR;
}

/**
 * sys_mkdir - create directory.
 *
 * Description:
 * Creates a new folder. If path is longer than 8 chars uses mkdrip instead. internally.
 *
 * Return: STAUS_ERROR || STATUS_OK.
 */
static int32_t sys_mkdir(struct registers *r) {
    DEBUG_SYSCALL("[SYSCALL][SYS_MKDIR]\n");
    const char *path = (char *)r->ebx;
    uint32_t len     = r->ecx;
    DEBUG_SYSCALL("[SYSCALL][SYS_MKDIR]: creating path %s with len: %d\n", path, len);

    task_t *current = scheduler_get_current_task();
    if (current == NULL) {
        DEBUG_SYSCALL("[SYSCALL][SYS_MKDIR]: no current task found\n");
        return STATUS_ERROR;
    }

    ledger_add_fs_req(current->pid, CREATE, 0, path, NULL, len, O_CREAT);
    scheduler_set_task_state(TASK_BLOCKED);
    scheduler_yield(r);

    return ledger_collect(current->pid, fs_task_pid, NULL);
}

static int32_t sys_getdents(struct registers *r) {
    DEBUG_SYSCALL("[SYSCALL][SYS_GETDENTS]\n");
    char *buffer    = (char *)r->ebx;
    uint32_t len    = r->ecx;
    char *path      = (char *)r->edx;

    task_t *current = scheduler_get_current_task();
    if (current == NULL) {
        DEBUG_SYSCALL("[SYSCALL][SYS_GETDENTS]: no current task found\n");
        return STATUS_ERROR;
    }

    ledger_add_fs_req(current->pid, LIST, 0, path, buffer, len, O_RDONLY);
    scheduler_set_task_state(TASK_BLOCKED);
    scheduler_yield(r);

    return ledger_collect(current->pid, fs_task_pid, buffer);
}

static int32_t sys_idle(struct registers *r) {
    (void)r;
    while (1) __asm__ __volatile__("sti; hlt");
    return STATUS_OK;
}

static int32_t sys_yield(struct registers *r) {
    r->eax = STATUS_OK;
    scheduler_yield(r);
    return STATUS_OK;
}

static int32_t sys_kill(struct registers *r) {
    task_t *current     = scheduler_get_current_task();
    task_t *target_task = task_get((uint32_t)r->ebx);

    if (current == NULL || target_task == NULL) {
        ERROR("[SYSCALL][SYS_KILL]: Target was invalid or current task was not found\n");
        return STATUS_ERROR;
    }

    DEBUG_SYSCALL("[SYSCALL][SYS_KILL]: Killing target: %s\n", target_task->name);

    if (!(target_task->pid >= CLERK_COUNT)) {
        return STATUS_ERROR;
    }

    target_task->state = TASK_DEAD;

    DEBUG_SYSCALL("[SYSCALL][SYS_KILL]: Requesting fs_task release allocated memory\n");
    scheduler_set_task_state(TASK_BLOCKED);
    ledger_add_fs_free_req(current->pid, target_task->pid);
    scheduler_yield(r);
    ledger_collect(current->pid, fs_task_pid, NULL);

    DEBUG_SYSCALL("[SYSCALL][SYS_KILL]: Requesting gui_task to release allocated memory\n");
    scheduler_set_task_state(TASK_BLOCKED);
    ledger_add_gui_free_req(current->pid, target_task->pid);
    scheduler_yield(r);
    ledger_collect(current->pid, gui_task_pid, NULL);

    DEBUG_SYSCALL("[SYSCALL][SYS_KILL]: Requesting reaper to kill the task\n");
    scheduler_set_task_state(TASK_BLOCKED);
    ledger_add_reaper_req(current->pid, target_task->pid);
    scheduler_yield(r);
    return ledger_collect(current->pid, reaper_task_pid, NULL);
}

/**
* change_keyboard_focus - when user task wishes to switch to another task.

* Description:
* This functions routes the keyboard to be directed to window that the user wants to be "active"
* creating the illusion of switching active windows. But in reality all windows are active and only the
* tasks that gets keyboard access if switched
*
* Context: Function was made so I could change between operator task and other tasks
* Return: status_ok || status_error.
*/
static int change_keyboard_focus(uint32_t target_pid) {
    DEBUG_SYSCALL("[SYSCALL][CKF]\n");
    task_t *current = scheduler_get_current_task();

    if (!(target_pid >= CLERK_COUNT || target_pid >= MAX_TASKS)) {
        return STATUS_ERROR;
    }

    if (current == NULL) {
        return STATUS_ERROR;
    }

    if (keyboard_replace_cur_foreground_pid(target_pid) == STATUS_ERROR) {
        DEBUG_SYSCALL("[SYS_CKF]: Could not replace current foreground pid. Aborting.\n");
        return STATUS_ERROR;
    }
    scheduler_set_task_state(TASK_BLOCKED);
    scheduler_wake_task(target_pid);

    return STATUS_OK;
}

/**
 * sys_configure_window - When userspace task wants to make changes to their window.
 *
 * Description:
 * There are 4 configure operations that can be made to window
 * first one is to create the window so that it is shown on the screen
 * second one is to paint it tho atm it does not do anything
 * third one is to move the window to another location on the screen.
 * fourth one sets the current window as the operator.
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
static int32_t sys_window(struct registers *r) {
    DEBUG_SYSCALL("[SYSCALL][CONWI]\n");
    uint32_t operation = r->ebx;
    task_t *current    = scheduler_get_current_task();

    if (current == NULL) {
        return STATUS_ERROR;
    }

    switch (operation) {
    case CREATE:
        current->state = TASK_BLOCKED;
        ledger_add_gui_req(current->pid, CREATE, r->ecx, r->edx, r->esi, r->edi, 0, 0, NULL);
        goto yield_collect;
    case PAINT_WINDOW:
    case SCROLL_DOWN:
        current->state = TASK_BLOCKED;
        ledger_add_gui_req(current->pid, operation, r->ecx, r->edx, r->esi, r->edi, 0, 0, NULL);
        goto yield_collect;
    case MOVE:
        current->state = TASK_BLOCKED;
        ledger_add_gui_req(current->pid, MOVE, 0, 0, r->ecx, r->edx, 0, 0, NULL);
        goto yield_collect;
    case BG_COLOR:
    case FG_COLOR:
        current->state = TASK_BLOCKED;
        ledger_ch_gui_color(current->pid, operation, r->ecx);
        goto yield_collect;
    case DRAW: {
        current->state          = TASK_BLOCKED;
        gui_params_pack *params = (gui_params_pack *)r->ecx;

        if (params == NULL) {
            return STATUS_ERROR;
        }

        ledger_add_gui_req(current->pid, DRAW, params->width, params->height, params->x,
                           params->y, params->scale, 0, params->pixels);
        goto yield_collect;
    }
    case WRITE_AT:
        current->state = TASK_BLOCKED;
        ledger_add_gui_text(current->pid, WRITE_AT, r->ecx, r->edx, r->esi, (const char *)r->edi);
        goto yield_collect;
    case SET_OPERATOR:

        return keyboard_set_operator_pid(current->pid);
    case CH_ACT_W:
        return change_keyboard_focus(r->ecx);
    default:
        current->state = TASK_RUNNING;
        break;
    }
    return STATUS_ERROR;

yield_collect:
    scheduler_yield(r);
    return ledger_collect(current->pid, gui_task_pid, NULL);
}

void syscall_dispatch(struct registers *r) {
    if (r->eax >= MAX_SYSCALLS || syscall_table[r->eax] == NULL) {
        r->eax = STATUS_ERROR;
        return;
    }
    r->eax = syscall_table[r->eax](r);
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
}
