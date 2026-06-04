#include "fs_task.h"
#include "blankie.h"
#include "hail_mary.h"

/*
* Fs_task
* Design & Implementation: A.H, 2026
*/

/*
* This file contains the implementation of filesystem_task. Its job is to talk to filesystem driver. For now it is hardcoded to be fat32 but can be
* expanded for other filesystems once the need arises.
*
* The basic idea is that it performs crud operations for userspace tasks. Taking in requests via syscalls. After completing said request
* it wakes the caller and if everything is completed it follows the blankie protocol.
*/

int request_queue_count = 0;
int current_req_index = -1;
fs_mailbox_queue *request_queue[MAX_TASKS];
fd_entry_t *fd_entry_table[MAX_TASKS];


/*
* 
*/
static void fs_remove_from_queue(fs_mailbox_queue *req) {
  DEBUG("[FS_TASK][REMOVE]: Starting on removing\n");
  if (request_queue_count == 0) {
      DEBUG("[FS_TASK][REMOVE]: Req queue count is 0\n");
      return;
  }

  if(req == NULL) {
    DEBUG("[FS_TASK][REMOVE]: Did not find a request to remove\n");
    return;
  }
  
  for (int i = 0; i < request_queue_count - 1; i++) {
    request_queue[i] = request_queue[i + 1];
  }
  
  request_queue[request_queue_count - 1] = NULL;
  request_queue_count--;
  
  
  DEBUG("[FS_TASK][REMOVE]: Freeing request heap memory\n");
  kfree(req);
  DEBUG("[FS_TASK][REMOVE]: Removing complete\n");
  
}

/*
* This functions was inspired from schedulers next task find function.
* 
*/
static fs_mailbox_queue *find_next_request() {
  if (request_queue_count == 0) return NULL;

  for(int i = 0; i < request_queue_count; i++) {
    if(request_queue[i] != NULL && (request_queue[i]->status == PENDING || request_queue[i]->status == IN_PROGRESS)) {
      current_req_index = i;
      return request_queue[i];
    }
  }

  for(int i = 0; i < request_queue_count; i++) {
    if(request_queue[i] != NULL && request_queue[i]->status == TERMINATED) {
      return request_queue[i];
    }
  }

  //DEBUG("[FS_TASK][NEXT_REQUEST]: could not find new request\n");
  return NULL;
}

/*
* Main task loop of fs_task the algorithm
*/
void fs_task_loop() {
  //DEBUG("[FS_TASK]: \n");
    while(1) {
      if(request_queue_count > 0) {
        fs_mailbox_queue *request = find_next_request();
        if(request != NULL) {
          if(request->status == PENDING || request->status == IN_PROGRESS) {
            //__asm__ __volatile__("cli");
            DEBUG("[FS_TASK][LOOP]: handling request\n");
            fs_handle_request(request);
            //__asm__ __volatile__("sti");
          }

          //__asm__ __volatile__("cli");
          if(request->status == TERMINATED) {
            DEBUG("[FS_TASK][LOOP]: Request is complete. Removing it\n");
            fs_remove_from_queue(request);
            request = NULL;
          }
          //__asm__ __volatile__("sti");
        }
      }


    /*
    * if(virt file needs servicing)
    * calculate next possible cluster/file
    * close / delete
    */

    if (request_queue_count == 0) {
        blankie_activate(fs_task_pid);
    }
  }
}


void fs_recovery() {
  ERROR("[FS_TASK][RECOVERY]: PROTOCOL HAIL MARY LAUNCHED\n");
  if(current_req_index == -1) {
    ERROR("[FS_TASK][RECOVERY]: No freeing needed\n");
    return;
  }
  fs_mailbox_queue *req = request_queue[current_req_index];
  if(req == NULL) return;
  scheduler_wake_task(req->caller_pid);
  fs_remove_from_queue(req);
  blankie_activate(fs_task_pid);
}

void fs_init(const task_t *fs_task) {
  register_hail_mary_function(fs_task_pid, fs_recovery);
  blankie_register(fs_task_pid, fs_task->context.eip, fs_task->kernel_stack);
}
