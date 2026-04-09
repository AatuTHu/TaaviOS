#include "sched.h"
#include "klog.h"
#include "config.h"
#include "kstring.h"
#include <stddef.h>

static proc_t *procs[MAX_PROCESSES];
static int proc_count = 0;
static int current_idx = -1;


void scheduler_tick(struct registers *r) {
    if(proc_count == 0) {
        //DEBUG("No processes added");
        return;
    } 
    if(current_idx == -1) {
        //DEBUG("No processes added");
        return;
    }

    if(procs[current_idx]->started) {
        memcpy(&procs[current_idx]->context, r, sizeof(struct registers));
        procs[current_idx]->useresp = r->esp;
    }

    procs[current_idx]->started = 1;
    procs[current_idx]->state = PROCESS_READY;

    int next_idx = -1;
    for (int i = 1; i <= proc_count; i++) {
        int candidate = (current_idx + i) % proc_count;
        if (procs[candidate]->state == PROCESS_READY) {
            next_idx = candidate;
            break;
        }
    }
    
    if (next_idx == -1) return;
    
    current_idx = next_idx;
    proc_t *next = procs[current_idx];
    next->state = PROCESS_RUNNING;
    
    /* Update CPU state for the new process */
    tss_set_kernel_stack(next->kernel_stack);
    vmm_switch(next->page_dir);

     
    memcpy(r, &next->context, sizeof(struct registers));
    r->esp = procs[current_idx]->useresp;
}

void scheduler_add(proc_t *proc) {
    if(proc_count >= MAX_PROCESSES) {
        DEBUG("[ERROR] : too many processes added to scheduler\n");
        return;
    }
    procs[proc_count] = proc;
    proc_count++;

    if(current_idx == -1) {
        current_idx = 0;
    }
}

proc_t *scheduler_get_current() {
    if(current_idx == -1) {
        DEBUG("[ERROR] : no processes added to scheduler yet\n");
        return NULL;
    }
    return procs[current_idx];
}

void scheduler_init() {
    DEBUG("SCHEDULER INITIALIZED\n");
}