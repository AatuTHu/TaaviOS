#include "fs_task.h"
#include "blankie.h"
#include "hail_mary.h"

/*
* Fs_task
* Design & Implementation: A.H, 2026
*/

/*
* This is the source file where the function loop, init, find_next_req and remove are located
*
*
*/

int request_queue_count = 0;
int current_req_index = -1;
fs_mailbox_queue *request_queue[MAX_TASKS];
fd_entry_t *fd_entry_table[MAX_TASKS];


/*
* This function takes in the request that is selected during loop phase.
* There is two ways of getting in here. If fs_next_request dosn't find pending or in_progress requests
* it tries to find a terminated one. The other way is thru fs_recovery which is linked to hail mary protocol.
*/
static void fs_remove_from_queue(fs_mailbox_queue *req) {
  DEBUG("[FS_TASK][REMOVE]: Starting on removing\n");
  if (request_queue_count == 0 || current_req_index == -1) {
      DEBUG("[FS_TASK][REMOVE]: Req queue count is 0\n");
      return;
  }
  
  if(req == NULL) return;

  //current req index is set when a task is selected
  request_queue[current_req_index] = NULL;
  kfree(req);

  //Shift the array starting starting from current request index one to the left.
  //For example i = (cri = 1); 1 < (rqc = 5); i++; 
  //      rq[1] = rq[1 + 1];
  //      next iteration rq[2] = rq[2 + 1];
  //      next iteration rq[3] = rq[3 + 1]; 
  //      until we reach the end and leave the last index as NULL
  for (int i = current_req_index; i < request_queue_count - 1; i++) {
    request_queue[i] = request_queue[i + 1];
  }
  
  //After shifting we delete the last index
  request_queue[request_queue_count - 1] = NULL;
  request_queue_count--;
  //set crq as back to -1 cause the index has been deleted.
  current_req_index = -1;
  
  DEBUG("[FS_TASK][REMOVE]: Freeing request heap memory\n");
  DEBUG("[FS_TASK][REMOVE]: Removing complete\n");
  
}

/*
* This functions was inspired from schedulers next task find function.
* It tries to find the index of the first in_progress request, 
* if none is found it tries to find pending. Lastly it tries to find a terminated req
* it can be cleaned away. If it find any of these it returns index of the selected request
*/
static int find_next_request() {
  if (request_queue_count == 0) return STATUS_ERROR;

  for(int i = 0; i < request_queue_count; i++) {
    if(request_queue[i] != NULL && request_queue[i]->status == IN_PROGRESS) {
      return current_req_index = i;
    }
  }

  for(int i = 0; i < request_queue_count; i++) {
    if(request_queue[i] != NULL && request_queue[i]->status == PENDING) {
      return current_req_index = i;
    }
  }

  for(int i = 0; i < request_queue_count; i++) {
    if(request_queue[i] != NULL && request_queue[i]->status == TERMINATED) {
       return current_req_index = i;
    }
  }

  //DEBUG("[FS_TASK][NEXT_REQUEST]: could not find new request\n");
  return INVALID_IDX;
}

/*
* Main task loop of fs_task the algorithm. This is the entry point of fs_task, It alwasy starts from here when woken from sleep
*/
void fs_task_loop() {
  //DEBUG("[FS_TASK]: \n");
    while(1) {
      int index = find_next_request();
      if(request_queue_count > 0 && index != INVALID_IDX) {
        fs_mailbox_queue *request = request_queue[index];
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

    //This if forces the clerk to iterate over and over again until all request are deleted.
    //This could lead to infinite loop now that I think of it. -> need to thin solution this. 
    if (request_queue_count == 0) { 
        blankie_activate(fs_task_pid);
    }
  }
}


//Hail mary function which is launced should the fs_task do something severely bad.
//I'm thinking this deletes the request it was doing as a safe measure. After it launch the blankie and get new stack and restart.
void fs_recovery() {
  DEBUG("[FS_TASK][RECOVERY]: PROTOCOL HAIL MARY LAUNCHED\n");
  if(current_req_index != -1) {
    fs_mailbox_queue *req = request_queue[current_req_index];
    DEBUG("[FS_TASK][RECOVERY]: No freeing needed\n");
    if(req != NULL) {
      scheduler_wake_task(req->caller_pid);
      fs_remove_from_queue(req);
    }
  }
  blankie_activate(fs_task_pid);
}

void fs_init(const task_t *fs_task) {
  register_hail_mary_function(fs_task_pid, fs_recovery);
  blankie_register(fs_task_pid, fs_task->context.eip, fs_task->kernel_stack);
}
