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
#include "kernel_idle.h"


static syscall_fn_t syscall_table[MAX_SYSCALLS];

static int32_t sys_exit(struct registers *r) {
    DEBUG("[SYSCALL][SYSEXIT]\n");
    //vmm_switch((page_directory_t *)kernel_page_dir); 
    //Switch back to kernels own directory so that the processors dosn't stay in destroyd proc dir.
    //We don't actually remove anything so this is not necessery.
    scheduler_kill_task();
    
    //This should be in the scheduler tick? Like maeby if scheduler cant find any tasks t orun then go to idle
    int remaining_tasks = scheduler_get_task_count();

    if(remaining_tasks == 0) {
        //NOW I GET IT!!!!! WE SET REGISTER TO SAFE VALUES 
        r->eip  = (uint32_t)kernel_idle;
        r->useresp = 0; 
        r->cs      = SEG_KERNEL_CODE;
        r->ss      = SEG_KERNEL_DATA;
        return 0;
    }

    int next_idx = scheduler_find_next();
    scheduler_switch_context(r, next_idx);

    return 0;
}


static int32_t sys_write(struct registers *r) {
    DEBUG("[SYSCALL][SYS_WRITE]\n");
    int fd       = r->ebx;
    char *buf    = (char *)r->ecx; //ecx has the params?
    uint32_t len = r->edx; //irrelevant in our case. but it would hold the lenght of the message
    
    if (fd == 1 || fd == 2) { // STDOUT or STDERR
        vga_write(buf);
    }
    return len;
}

static int32_t sys_getpid(struct registers *r) {
    (void)r;
    proc_t *proc = scheduler_get_current();
    return (proc) ? (int32_t)proc->pid : -1;
}

static int32_t sys_read(struct registers *r) {
    DEBUG("[SYSCALL][SYS_READ]\n");
    int fd    = r->ebx;
    char *buf = (char *)r->ecx;
    
    if (fd != 0) {
        DEBUG("[SYSCALL][SYS_READ]: fd not 0\n");
        return -1; // Only STDIN (0) supported currently
    }
    
    proc_t *current = scheduler_get_current();
    if (!current) {
        DEBUG("[SYSCALL][SYS_READ]: current task not found\n");
        return -1;
    }

    int nread = 0;
    if (nread == 0) {
        DEBUG("[SYSCALL][SYS_READ]: nread = 0 blocking\n");
        scheduler_block_task(r); // Give up the remaining time slice
        int next_idx = scheduler_find_next();
        if(next_idx != -1) {
            DEBUG("[SYSCALL][SYS_READ]: next task found, switching\n");
            scheduler_switch_context(r, next_idx);
        } else {
            DEBUG("[SYSCALL][SYS_READ]: no next task found, setting current task to ready\n");
            scheduler_set_task_ready();
        }
        return nread;
    }

    return nread;
}

void syscall_init() {
    DEBUG("[SYSCALL] INITIALIZING SYSCALLS\n");
    for(uint8_t i = 0; i < MAX_SYSCALLS; i++) syscall_table[i] = NULL;
    
    syscall_table[SYS_EXIT]     = sys_exit;
    syscall_table[SYS_WRITE]    = sys_write;
    syscall_table[SYS_GETPID]   = sys_getpid;
    //syscall_table[SYS_EXEC]     = sys_exec;
    syscall_table[SYS_READ]     = sys_read;
}

void syscall_dispatch(struct registers *r) {
    if (r->eax >= MAX_SYSCALLS || syscall_table[r->eax] == NULL) {
        r->eax = -1;
        return;
    }

    r->eax = syscall_table[r->eax](r);
}