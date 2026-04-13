#include "syscall.h"
#include "vga.h"
#include "proc.h"
#include "sched.h"
#include "paging.h"
#include "gdt.h"
#include "pit.h"
#include "klog.h"
#include "elf.h"


static syscall_fn_t syscall_table[MAX_SYSCALLS];


static int32_t sys_exit(struct registers *r) {

}


static int32_t sys_write(struct registers *r) {
    int fd       = r->ebx;
    char *buf    = (char *)r->ecx;
    uint32_t len = r->edx;
    
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