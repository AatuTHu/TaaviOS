#include "fs_task.h"
#include "fat32.h"
#include "sched.h"
#include "klog.h"
#include "kstring.h"

static int request_queue_count = -1;
static int last_request_index = -1;
static fs_task_queue_t *request_queue[MAX_PROCESSES];
static const int FS_TASK_QUEUE_SIZE = MAX_PROCESSES * sizeof(fs_task_queue_t);
static const int FS_TASK_BUFFER_SIZE = 4096;
static const int FS_TASK_REGION_SIZE = FS_TASK_QUEUE_SIZE + FS_TASK_BUFFER_SIZE;
static uint32_t mem_start;
static uint32_t mem_end;

int check_boundaries(void *ptr, uint32_t size) {
    uint32_t addr = (uint32_t)ptr;
    if(addr < mem_start || addr + size > mem_end) {
        return STATUS_ERROR;
    }
    return STATUS_OK;
}

void fs_wake_task(fs_task_queue_t *req) {
    scheduler_wake_task(req->caller_pid);
    int next_idx = scheduler_get_idx_off_pid(req->caller_pid);
    proc_t *self = scheduler_get_current_task();
    scheduler_switch_context(&self->context, next_idx);
}

int fs_handle_request(fs_task_queue_t *req) {

    //stub. to. be. continued
    req->status = COMPLETE;
    fs_wake_task(req);
}

/*
* 
*/
void fs_remove_from_queue() {
    request_queue[last_request_index] = NULL;

    DEBUG("[FS_TASK][REMOVE]: Shifting rest of the array to the left\n");
    for (int i = last_request_index; i < request_queue_count - 1; i++) {
        request_queue[i] = request_queue[i + 1];
    }

    request_queue[request_queue_count - 1] = NULL;
    request_queue_count--;
    last_request_index = -1;
}


//reguest types can be 1 write, 2 read, 3 update and 4 delete
//Pid is the id from the caller and target is naturally the file
int add_request_to_queue(uint32_t pid, uint8_t request_type, const char* target) {
    fs_task_queue_t *new_request = (fs_task_queue_t *)kmalloc(sizeof(fs_task_queue_t));

    new_request->caller_pid = pid;
    new_request->request_type = request_type;
    strncpy(new_request->path, target, sizeof(new_request->path));
    new_request->status = PENDING;

    request_queue_count++;
    request_queue[request_queue_count] = new_request;
}

fs_task_queue_t *find_next_request() {

    for(int i = 0; i < request_queue_count; i++) {
        if(request_queue[i]->status == IN_PROGRESS) {
            last_request_index = i;
            return request_queue[i];
        }
    }

    for(int i = 0; i < request_queue_count; i++) {
        if(request_queue[i]->status == PENDING) {
            last_request_index = i;
            return request_queue[i];
        }
    }

    for(int i = 0; i < request_queue_count; i++) {
        if(request_queue[i]->status == COMPLETE) {
            last_request_index = i;
            return request_queue[i];
        }
    }

    DEBUG("[FS_TASK][NEXT_REQUEST]: could not find new request\n");
    return NULL;
}

void fs_task_loop() {
    DEBUG("[FS_TASK]: First time running\n");
    while(1) {
        if(request_queue_count != -1) {
            fs_task_queue_t *request = NULL;
    
            if(last_request_index == -1) {
                request = find_next_request();
            } else {
                request = request_queue[last_request_index];
            }
    
            if(request != NULL) {
                if(request->status == PENDING || request->status == IN_PROGRESS) {
                    DEBUG("[FS_TASK][LOOP]: handling request\n");
                    fs_handle_request(request);
                }
                
                if(request->status == COMPLETE) {
                    DEBUG("[FS_TASK][LOOP]: Request is complete. Removing it\n");
                    fs_remove_from_queue();
                }
                
            } else {
                DEBUG("[FS_TASK][LOOP]: No new request found.\n");
            }
            
        }
        
        
        /*
        * if(virt file needs servicing)
        * calculate next possible cluster/file
        * close / delete
        */
       
       DEBUG("[FS_TASK][LOOP]: No requests or servicing required. Activating blankie protocol\n");
       scheduler_set_task_sleeping();
       proc_t *self = scheduler_get_current_task();
       int next_idx = scheduler_find_next_task();
       scheduler_switch_context(&self->context, next_idx);
    }
}

void fs_init() {
    mem_start = (uint32_t *)kmalloc(FS_TASK_REGION_SIZE);
    mem_end   = mem_start + FS_TASK_REGION_SIZE; 
}
