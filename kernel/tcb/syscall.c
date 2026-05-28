#include "syscall.h"
#include "vga.h"
#include "task.h"
#include "sched.h"
#include "klog.h"
#include "elf.h"
#include "vmm.h"
#include "config.h"
#include "keyboard.h"
#include "idle_task.h"
#include "fs_task.h"


static syscall_fn_t syscall_table[MAX_SYSCALLS];

static int32_t sys_exit(struct registers *r) {
    __asm__ __volatile__("cli");
    DEBUG("[SYSCALL][SYSEXIT]\n");

    scheduler_kill_task();
    
    task_t *current = scheduler_get_current_task();

    if(keyboard_get_foreground_pid() == current->pid) {
        DEBUG("[SYSCALL][SYSEXIT]: releasing foregroundpid\n");
        keyboard_set_foreground_pid(-1);
    }

    scheduler_switch_context(r, scheduler_find_next_task());
    __asm__ __volatile__("sti");
    return 0;
} //sys_exit


static int32_t sys_write(struct registers *r) {
    int fd       = r->ebx;
    char *buf    = (char *)r->ecx; //ecx has the string
    uint32_t len = r->edx; //irrelevant in our case. but it would hold the length of the message
    
    switch (fd)
    {
    case 1:
        vga_write(buf);
        break;
    case 2:
        vga_write(buf);
        break;
    case 3:
        
    default:
        task_t *current = scheduler_get_current_task();
        scheduler_block_task();
        add_request_to_queue(current->pid, WRITE, fd," ",buf);
        break;
    }

    return len;
} //sys_write

static int32_t sys_getpid(struct registers *r) {
    (void)r;
    task_t *task = scheduler_get_current_task();
    return (task) ? (int32_t)task->pid : -1;
} //sys_getpid

/*
* 1. get current task. See if there is a foreground pid set. If not then set it as the foregroundpid. Otherwhise add it to keyboard waiting queue and switch context
* 2. get fd and the buf. See if keyborad buffer has something for it. It it has return it if not then set caller task blocked, set eax to 0 and yield
*/
static int32_t sys_read(struct registers *r) {

    int fd    = r->ebx;
    char *buf = (char *)r->ecx;

    switch (fd)
    {
    case 0: //stdin
         task_t *current = scheduler_get_current_task();

        if (!current) {
            DEBUG("[SYSCALL][SYS_READ]: current task not found\n");
            return -1;
        }
        
        int nread = keyboard_read_from_buffer(buf, current->pid);
        if (nread > 0) return nread; //buffer had something so 


        if(current->state != TASK_BLOCKED) {
            scheduler_block_task();
            r->eax = 0;
            scheduler_switch_context(r, scheduler_find_next_task());
        }
        break;
    
    default:
        break;
    }

   
    return 0;
    
} //sys_read

/*
*   This is a hack function. Does not really execute elf binaries. I made it for now so that I could test multitasking. NEEDS TO BE FIXED
*/
static int sys_exec(struct registers *r) {
    DEBUG("[SYSCALL][SYS_EXEC]\n");
    char *filename = (char *)r->ecx;
    task_t *task = get_task_by_name(filename);
    if(task == NULL) {
        return -1;
    }
    
    scheduler_add(task);
    scheduler_switch_context(r, scheduler_find_next_task());
    return 0;
} //sys_exec

static int32_t sys_yield(struct registers *r) {
    //r->eax = 0;
    scheduler_switch_context(r, scheduler_find_next_task());
    return 0;
} //sys_yield

static int32_t sys_idle(struct registers *r) {
    while(1) __asm__ __volatile__("sti; hlt");
    return 0;
} //sys_idle

void syscall_init() {
    DEBUG("[SYSCALL] INITIALIZING SYSCALLS\n");
    for(uint32_t i = 0; i < MAX_SYSCALLS; i++) syscall_table[i] = NULL;
    syscall_table[SYS_EXIT]     = sys_exit;
    syscall_table[SYS_WRITE]    = sys_write;
    syscall_table[SYS_GETPID]   = sys_getpid;
    syscall_table[SYS_EXEC]     = sys_exec;
    syscall_table[SYS_READ]     = sys_read;
    syscall_table[SYS_IDLE]     = sys_idle;
    syscall_table[SYS_YIELD]    = sys_yield;
}

void syscall_dispatch(struct registers *r) {
    if (r->eax >= MAX_SYSCALLS || syscall_table[r->eax] == NULL) {
        r->eax = -1;
        return;
    }

    r->eax = syscall_table[r->eax](r);
}