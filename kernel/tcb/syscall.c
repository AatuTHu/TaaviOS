#include "syscall.h"
#include "config.h"
#include "elf.h"
#include "fat32.h"
#include "fs_task.h"
#include "keyboard.h"
#include "klog.h"
#include "sched.h"
#include "task.h"
#include "vga.h"
#include "vmm.h"

static syscall_fn_t syscall_table[MAX_SYSCALLS];

static int32_t sys_exit(struct registers *r) {
    // DEBUG("[SYSCALL][SYSEXIT]\n");
    const task_t *current = scheduler_get_current_task();

    if (current == NULL) {
        // DEBUG("[SYSCALL][SYSEXIT]: current task not found\n");
        return STATUS_ERROR;
    }

    if (current->state == TASK_DEAD) {
        return STATUS_OK;
    }

    if (keyboard_get_foreground_pid() == (int)current->pid) {
        // DEBUG("[SYSCALL][SYSEXIT]: releasing foregroundpid\n");
        keyboard_set_foreground_pid(-1);
    }

    scheduler_set_task_state(TASK_DEAD);
    scheduler_yield(r);

    return STATUS_OK;
} // sys_exit

/*
 * 1. get current task. See if there is a foreground pid set. If not then set it
 * as the foreground pid. Otherwhise add it to keyboard waiting queue and switch
 * context
 * 2. get fd and the buf. See if keyborad buffer has something for it. It it has
 * return it if not then set caller task blocked, set eax to 0 and yield
 */
static int32_t sys_read(struct registers *r) {
    int fd                = r->ebx;
    char *buf             = (char *)r->ecx;
    uint32_t buff_size    = (uint32_t)r->edx;
    const task_t *current = scheduler_get_current_task();

    if (fd == 0) { // stdin
        int nread = 0;
        while ((nread = keyboard_read_from_buffer(buf, current->pid)) == 0) {
            scheduler_set_task_state(TASK_BLOCKED);
            scheduler_yield(r);
        }
        return nread;
    } else {
        add_request_to_queue(current->pid, READ, fd, NULL, NULL, buff_size,
                             O_RDONLY);
        scheduler_set_task_state(TASK_BLOCKED);
        scheduler_yield(r);

        ////DEBUG("[SYSCALL][SYS_READ]: %s awoken. Collecting\n",
        /// scheduler_get_current_task()->name);
        if (collect_request(current->pid, buf) == STATUS_ERROR) {
            // DEBUG("[SYSCALL][SYS_READ]: Collecting results went wrong %s\n",
            // scheduler_get_current_task()->name);
            return STATUS_ERROR;
        }
        DEBUG("[SYSCALL][SYS_READ]: Resulting buffer: %s\n", buf);
    }

    return STATUS_OK;

} // sys_read

static int32_t sys_write(struct registers *r) {
    int fd          = r->ebx;
    const char *buf = (char *)r->ecx; // ecx has the string
    uint32_t len    = r->edx; // irrelevant in our case. but it would hold the
                              // length of the message

    switch (fd) {
    case 1:
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_write(buf);
        r->eax = STATUS_OK;
        break;
    case 2:
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        ERROR(buf);
        r->eax = STATUS_OK;
        break;
    default:
        const task_t *current = scheduler_get_current_task();
        add_request_to_queue(current->pid, WRITE, fd, NULL, buf, len, O_WRONLY);
        scheduler_set_task_state(TASK_BLOCKED);
        scheduler_yield(r);

        int new_offset = collect_request(current->pid, NULL);

        return new_offset;
    }

    return STATUS_OK;
} // sys_write

static int32_t sys_open(struct registers *r) {
    // DEBUG("[SYSCALL][SYS_OPEN]\n");
    const char *path      = (char *)r->ebx;
    uint32_t flags        = r->ecx;
    const task_t *current = scheduler_get_current_task();

    if (current == NULL) {
        // DEBUG("[SYSCALL][SYS_OPEN]: current task not found\n");
        return STATUS_ERROR;
    }

    add_request_to_queue(current->pid, OPEN, 0, path, NULL, 0, flags);
    scheduler_set_task_state(TASK_BLOCKED);
    scheduler_yield(r);

    int fd = collect_request(current->pid, NULL);

    if (fd == STATUS_ERROR) {
        // DEBUG("[SYSCALL][SYS_OPEN]: Invalid fd, reader: %s\n",
        // scheduler_get_current_task()->name);
    }
    // DEBUG("[SYSCALL][SYS_OPEN]: Returning fd: %d\n", fd);
    return fd;
}

static int32_t sys_close(struct registers *r) {
    DEBUG("[SYSCALL][SYS_CLOSE]\n");
    int fd                = r->ebx;
    const task_t *current = scheduler_get_current_task();
    if (current == NULL) {
        return STATUS_ERROR;
    }

    add_request_to_queue(current->pid, CLOSE, fd, NULL, NULL, 0, 0);
}

static int32_t sys_getpid(struct registers *r) {
    (void)r;
    const task_t *task = scheduler_get_current_task();
    return (task) ? (int32_t)task->pid : -1;
} // sys_getpid

/**
 * Sys_exec - Execute binaries.
 * @r: - current context registers
 *
 * Description:
 * This function find the file at the end of given path.
 * If it finds it, it read the contents of the file and then makes an elf binary
 * of it After that create page directory, a task and add it to scheduler
 *
 * Context: to execute tasks.
 * Return: STATUS_ERROR || STATUS_OK.
 */
static int32_t sys_exec(struct registers *r) {
    DEBUG("[SYSCALL][SYS_EXEC]\n");
    task_t *current      = scheduler_get_current_task();
    const char *filename = (char *)r->ecx;
    if (current == NULL || filename == NULL) {
        return STATUS_ERROR;
    }

    uint32_t file_cluster = 0;
    uint32_t file_size    = 0;
    char task_name[8];
    DEBUG("[SYSCALL][SYS_EXEC]: Trying to find %s\n", filename);
    if (fat32_find_file(filename, &file_cluster, &file_size, &task_name) ==
        STATUS_ERROR) {
        ERROR("[SYS_EXEC]: Did not find the file\n");
        return STATUS_ERROR;
    }

    DEBUG("[SYSCALL][SYS_EXEC]: continuing to read file with name %s\n",
          task_name);

    uint8_t *binary_buffer = (uint8_t *)kmalloc(file_size);
    if (fat32_read_file(file_cluster, file_size, binary_buffer) ==
        STATUS_ERROR) {
        ERROR("[SYS_EXEC]: Could not read the file\n");
        return STATUS_ERROR;
    }

    DEBUG("[SYSCALL][SYS_EXEC]: creating the task\n");

    page_directory_t *pd = vmm_create_directory();
    uint32_t entry       = elf_load(binary_buffer, pd);
    kfree(binary_buffer);
    task_t *task = task_create(-1, entry, task_name, pd, USER_TASK);

    scheduler_add(task);

    return STATUS_OK;
} // sys_exec

static int32_t sys_mkdir(struct registers *r) {
    DEBUG("[SYSCALL][SYS_MKDIR]\n");
    const char *path = (char *)r->ebx;
    DEBUG("[SYSCALL][SYS_MKDIR]: creating path %s\n", path);
    task_t *current = scheduler_get_current_task();
    if (current == NULL) {
        DEBUG("[SYSCALL][SYS_MKDIR]: no current task found\n");
        return STATUS_ERROR;
    }
    add_request_to_queue(current->pid, CREATE, 0, path, NULL, 0, O_CREAT);
    scheduler_set_task_state(TASK_BLOCKED);
    scheduler_yield(r);

    if (collect_request(current->pid, NULL) == STATUS_ERROR)
        return STATUS_ERROR;

    return STATUS_OK;
}

static int32_t sys_idle(struct registers *r) {
    (void)r;
    while (1) __asm__ __volatile__("sti; hlt");
    return STATUS_OK;
} // sys_idle

static int32_t sys_yield(struct registers *r) {
    // DEBUG("[SYSCALL][SYS_yield]: caller: %s\n",
    // scheduler_get_current_task()->name);
    r->eax = STATUS_OK;
    scheduler_yield(r);
    return STATUS_OK;
} // sys_yield

void syscall_dispatch(struct registers *r) {
    if (r->eax >= MAX_SYSCALLS || syscall_table[r->eax] == NULL) {
        r->eax = STATUS_ERROR;
        return;
    }

    r->eax = syscall_table[r->eax](r);
}

void syscall_init() {
    // DEBUG("[SYSCALL] INITIALIZING SYSCALLS\n");
    for (uint32_t i = 0; i < MAX_SYSCALLS; i++) syscall_table[i] = NULL;
    syscall_table[SYS_EXIT]   = sys_exit;
    syscall_table[SYS_WRITE]  = sys_write;
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_EXEC]   = sys_exec;
    syscall_table[SYS_READ]   = sys_read;
    syscall_table[SYS_IDLE]   = sys_idle;
    syscall_table[SYS_YIELD]  = sys_yield;
    syscall_table[SYS_OPEN]   = sys_open;
    syscall_table[SYS_CLOSE]  = sys_close;
    syscall_table[SYS_MKDIR]  = sys_mkdir;
}