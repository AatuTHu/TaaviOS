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
    vmm_switch(kernel_page_dir); //Switch back to kernels own directory so that the processors dosn't stay in destroyd proc dir.
    scheduler_remove();
    
    //This should be in the scheduler tick? Like maeby if scheduler cant find any tasks t orun then go to idle
    int remaining_tasks = scheduler_get_task_count();

    if(remaining_tasks == 0) {
        //NOW I GET IT!!!!! WE SET REGISTER TO SAFE VALUES 
        r->eip  = (uint32_t)kernel_idle;
        //r->useresp = 0; 
        r->cs      = SEG_KERNEL_CODE;
        r->ss      = SEG_KERNEL_DATA;
        return 0;
    }
    
    return 0;
}


static int32_t sys_write(struct registers *r) {
    int fd       = r->ebx;
    char *buf    = (char *)r->ecx; //ecx has the params?
    uint32_t len = r->edx; //irrelevant in our case. but it would hold the lenght of the message
    
    if (fd == 1 || fd == 2) { // STDOUT or STDERR
        vga_write(buf);
    }
    return len;
}

static int32_t sys_getpid(struct registers *r) {
    proc_t *proc = scheduler_get_current();
    return (proc) ? (int32_t)proc->pid : -1;
}

void syscall_init() {
    DEBUG("[SYSCALL] INITIALIZING SYSCALLS\n");
    for(uint8_t i = 0; i < MAX_SYSCALLS; i++) syscall_table[i] = NULL;
    
    syscall_table[SYS_EXIT]     = sys_exit;
    syscall_table[SYS_WRITE]    = sys_write;
    syscall_table[SYS_GETPID]   = sys_getpid;
    //syscall_table[SYS_EXEC]     = sys_exec;
    //syscall_table[SYS_READ]     = sys_read;
}

void syscall_dispatch(struct registers *r) {
    if (r->eax >= MAX_SYSCALLS || syscall_table[r->eax] == NULL) {
        r->eax = -1;
        return;
    }

    r->eax = syscall_table[r->eax](r);
}