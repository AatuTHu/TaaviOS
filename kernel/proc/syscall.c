#include "syscall.h"
#include "vga.h"
#include "proc.h"
#include "sched.h"
#include "paging.h"
#include "gdt.h"
#include "pit.h"
#include "klog.h"
#include "elf.h"
#include "vmm.h"
#include "config.h"
#include "keyboard.h"
#include "kernel_idle.h"


static syscall_fn_t syscall_table[MAX_SYSCALLS];

static int32_t sys_exit(struct registers *r) {
    DEBUG("[SYSCALL][SYSEXIT]\n");

    scheduler_kill_task();
    
    proc_t *current = scheduler_get_current_task();

    if(get_foreground_pid() == current->pid) {
        DEBUG("[SYSCALL][SYSEXIT]: releasing foregroundpid\n");
        set_foreground_pid(-1);
    }
    
    int remaining_tasks = scheduler_get_task_count();

    if(remaining_tasks == 0) {
        r->eip  = (uint32_t)kernel_idle;
        r->useresp = 0; 
        r->cs      = SEG_KERNEL_CODE;
        r->ss      = SEG_KERNEL_DATA;
        return 0;
    }

    int next_idx = scheduler_find_next_task();
    scheduler_switch_context(r, next_idx);

    return 0;
} //sys_exit


static int32_t sys_write(struct registers *r) {
    int fd       = r->ebx;
    char *buf    = (char *)r->ecx; //ecx has the string
    uint32_t len = r->edx; //irrelevant in our case. but it would hold the length of the message
    
    if (fd == 1 || fd == 2) { // STDOUT or STDERR
        vga_write(buf);
    }
    return len;
} //sys_write

static int32_t sys_getpid(struct registers *r) {
    (void)r;
    proc_t *proc = scheduler_get_current_task();
    return (proc) ? (int32_t)proc->pid : -1;
} //sys_getpid

static int32_t sys_read(struct registers *r) {

    proc_t *current = scheduler_get_current_task();

    if (!current) {
        DEBUG("[SYSCALL][SYS_READ]: current task not found\n");
        return -1;
    }

    if(get_foreground_pid() == -1) {
        set_foreground_pid(current->pid);
    }
    
    if(get_foreground_pid() != current->pid) {
        return -1;
    }

    int fd    = r->ebx;
    char *buf = (char *)r->ecx;
    
    if (fd != 0) {
        DEBUG("[SYSCALL][SYS_READ]: fd not 0\n");
        return -1; // Only STDIN (0) supported currently
    }
    

    int nread = read_from_keyboard_buffer(buf);
    if (nread > 0) {
        return nread;
    }

    if (nread <= 0) {
    scheduler_block_task(r);

    if (scheduler_has_runnable_task()) {
        int next_idx = scheduler_find_next_task();
        scheduler_switch_context(r, next_idx);
    } else {
        __asm__ volatile ("sti; hlt");
    }

    return 0;
    }
} //sys_read

static int sys_exec(struct registers *r) {
    DEBUG("[SYSCALL][SYS_EXEC]: filename: %s\n", r->ecx);
    char *filename = (char *)r->ecx;
    proc_t *proc = get_proc_by_name(filename);
    if(proc == NULL) {
        return 0;
    }
    
    int is_added = scheduler_does_exist(proc->pid);
    if(is_added == 0) {
        scheduler_add(proc);
        int next_idx = scheduler_find_next_task();
        scheduler_switch_context(r, next_idx);
        return 1;
    }

    return 0;
}

static int32_t sys_yield(struct registers *r) {
    int next_idx = scheduler_find_next_task();
    if(next_idx != -1) {
        scheduler_switch_context(r, next_idx);
    }
    return 0;
}

void syscall_init() {
    DEBUG("[SYSCALL] INITIALIZING SYSCALLS\n");
    for(uint32_t i = 0; i < MAX_SYSCALLS; i++) syscall_table[i] = NULL;
    
    syscall_table[SYS_EXIT]     = sys_exit;
    syscall_table[SYS_WRITE]    = sys_write;
    syscall_table[SYS_GETPID]   = sys_getpid;
    syscall_table[SYS_EXEC]     = sys_exec;
    syscall_table[SYS_READ]     = sys_read;
    syscall_table[SYS_YIELD]    = sys_yield;
}

void syscall_dispatch(struct registers *r) {
    if (r->eax >= MAX_SYSCALLS || syscall_table[r->eax] == NULL) {
        r->eax = -1;
        return;
    }

    r->eax = syscall_table[r->eax](r);
}