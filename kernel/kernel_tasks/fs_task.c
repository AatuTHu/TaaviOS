#include "fs_task.h"
#include "fat32.h"
#include "sched.h"
#include "klog.h"
#include "kstring.h"
#include "kmalloc.h"

/*
* Author: A.H, started 27.5.2026
*/

static int request_queue_count = -1;
static int last_request_index = -1;
static fs_mailbox_queue *request_queue[MAX_TASKS];
static const int FS_TASK_QUEUE_SIZE = MAX_TASKS * sizeof(fs_mailbox_queue);
static const int FS_TASK_TABLE_SIZE = 64 * sizeof(fd_entry_t);
static const int FS_TASK_BUFFER_SIZE = 4096;
static const int FS_TASK_REGION_SIZE = FS_TASK_QUEUE_SIZE + FS_TASK_BUFFER_SIZE + FS_TASK_TABLE_SIZE;
static uint32_t mem_start;
static uint32_t mem_end;

int check_boundaries(void *ptr, uint32_t size) {
    uint32_t addr = (uint32_t)ptr;
    if(addr < mem_start || addr + size > mem_end) {
        return STATUS_ERROR;
    }
    return STATUS_OK;
}

void fs_wake_task(fs_mailbox_queue *req) {
    scheduler_wake_task(req->caller_pid);
}

int fs_handle_request(fs_mailbox_queue *req) {

    //stub. to. be. continued
    req->status = COMPLETE;
}

/*
* 
*/
void fs_remove_from_queue() {
    DEBUG("[FS_TASK][REMOVE]: Freeing request heap memory\n");
    kfree(request_queue[last_request_index]);
    
    DEBUG("[FS_TASK][REMOVE]: Deleting at req_index and shifting rest of the array to the left\n");
    request_queue[last_request_index] = NULL;
    for (int i = last_request_index; i < request_queue_count - 1; i++) {
        request_queue[i] = request_queue[i + 1];
    }

    request_queue[request_queue_count - 1] = NULL;
    request_queue_count--;
    last_request_index = -1;
    DEBUG("[FS_TASK][REMOVE]: Removing complete\n");
}


//reguest types can be 1 write, 2 read, 3 update and 4 delete
//Pid is the id from the caller and path is naturally the file
int add_request_to_queue(uint32_t pid, operations_t type, uint32_t fd, const char* path, char *buf) {

    DEBUG("[FS_TASK][artq]: adding a request for fs_task\n");

    fs_mailbox_queue *new_request = (fs_mailbox_queue *)kmalloc(sizeof(fs_mailbox_queue));

    new_request->caller_pid = pid;
    new_request->request_type = type;
    strncpy(new_request->path, path, sizeof(new_request->path));
    new_request->fd = fd;
    new_request->status = PENDING;
    request_queue_count++;
    request_queue[request_queue_count] = new_request;
    
    scheduler_wake_task(1);

    DEBUG("[FS_TASK][artq]: pid: %d\n", pid);
    DEBUG("[FS_TASK][artq]: request_type: %d\n", type);
    DEBUG("[FS_TASK][artq]: fd: %d\n", fd);
    DEBUG("[FS_TASK][artq]: path: %s\n", path);
    DEBUG("[FS_TASK][artq]: buf: %s\n", buf);

    DEBUG("[FS_TASK][artq]: request added\n");
}

fs_mailbox_queue *find_next_request() {

    for(int i = 0; i <= request_queue_count; i++) {
        if(request_queue[i]->status == IN_PROGRESS) {
            last_request_index = i;
            return request_queue[i];
        }
    }

    for(int i = 0; i <= request_queue_count; i++) {
        if(request_queue[i]->status == PENDING) {
            last_request_index = i;
            return request_queue[i];
        }
    }

    for(int i = 0; i <= request_queue_count; i++) {
        if(request_queue[i]->status == COMPLETE) {
            last_request_index = i;
            return request_queue[i];
        }
    }

    DEBUG("[FS_TASK][NEXT_REQUEST]: could not find new request\n");
    return NULL;
}

void fs_task_loop() {
    DEBUG("[FS_TASK]: \n");
    while(1) {
        
        if(request_queue_count != -1) {
            fs_mailbox_queue *request = NULL;
    
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
                    fs_wake_task(request);
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
       
       //DEBUG("[FS_TASK][LOOP]: No requests or servicing required. Activating blankie protocol\n");
       scheduler_set_task_sleeping();
    }
}

void fs_init() {
    mem_start = (uint32_t *)kmalloc(FS_TASK_REGION_SIZE);
    mem_end   = mem_start + FS_TASK_REGION_SIZE; 
}
