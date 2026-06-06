#include "fs_task.h"

/*
* Fs_task
* Design & Implementation: A.H, 2026
*/

/*
* This file contains the public call functions that should be called from the sys_calls
*
*/

/*
* ADD_REQUEST_TO_QUEUE
* Takes the params sent to it via syscall and makes an work "order" for fs_task. 
*/
int add_request_to_queue(uint32_t pid, operations_t type, uint32_t fd, const char* path, const char *buf, uint32_t buffer_size, uint32_t flags) {
  //DEBUG("[FS_TASK][ADD_REQUEST]: adding a request for fs_task\n");

  fs_mailbox_queue *new_request = (fs_mailbox_queue*)kmalloc(sizeof(fs_mailbox_queue));

  if(new_request == NULL) {
      ERROR("[FS_TASK][ADD_REQUEST]: could on allocate new requestat this time. Aborting\n");
      scheduler_wake_task(pid);
      return STATUS_ERROR;
  }

  new_request->caller_pid = pid;
  DEBUG("[FS_TASK][ADD_REQUEST]: pid: %d\n", new_request->caller_pid);
  new_request->request_type = type;
  DEBUG("[FS_TASK][ADD_REQUEST]: request_type: %d\n", new_request->request_type);
  new_request->fd = fd;
  DEBUG("[FS_TASK][ADD_REQUEST]: fd: %d\n", new_request->fd);
  new_request->buffer_size = buffer_size;
  DEBUG("[FS_TASK][ADD_REQUEST]: buffer length: %d\n", new_request->buffer_size);
  new_request->flags = flags;
  DEBUG("[FS_TASK][ADD_REQUEST]: flags: %d\n", new_request->flags);
  
  if(path != NULL && type == OPEN) {
      strncpy(new_request->path, path, sizeof(new_request->path) - 1); //copy the path string to path
      new_request->path[sizeof(new_request->path) - 1] = '\0';         //end it ate 127. path is 128 long
      //DEBUG("[FS_TASK][ADD_REQUEST]: path: %s\n", new_request->path);
  }
  
  if(buf != NULL && type == WRITE) {
      strncpy(new_request->buf, buf, buffer_size);
      new_request->buf[buffer_size] = '\0';
      DEBUG("[FS_TASK][ADD_REQUEST]: buf: %s and buf length: %d\n", new_request->buf, buffer_size);
  }
  

  new_request->status = PENDING;
  request_queue[request_queue_count] = new_request;
  request_queue_count++;
  
  //DEBUG("[FS_TASK][ADD_REQUEST]: request added\n");
  task_t *fs_task = task_get(fs_task_pid);
  
  fs_task->priority = PRIORITY_HIGH; //set fs_task to be high so it is picked more frequently
  fs_task->state    = TASK_READY;   // set it ready so it can be picked at all

  return STATUS_OK;
}

/*
* This function gives the result to outside world. Mainly for the caller in syscall
*/
int collect_request(uint32_t pid, char *out) {
  __asm__ __volatile__("cli");
  //DEBUG("[FS_TASK][COLLECT_REQUEST]: Fetching request for %d\n", pid);
  
  
  for(int i = 0; i < request_queue_count; i++) {
    if(request_queue[i] != NULL && request_queue[i]->caller_pid == pid && request_queue[i]->status == COMPLETE) {
      //DEBUG("[FS_TASK][COLLECT_REQUEST]: Request found!\n");
      switch (request_queue[i]->request_type) {

      case OPEN:
        //DEBUG("[FS_TASK][COLLECT_REQUEST]: Returning fd: %d\n", request_queue[i]->fd);
        request_queue[i]->status = TERMINATED;
        __asm__ __volatile__("sti");
        return request_queue[i]->fd;  

      case READ:
        //DEBUG("[FS_TASK][COLLECT_REQUEST]: Returning read file to %d\n", pid);
        memcpy(out, request_queue[i]->buf, request_queue[i]->buffer_size);
        request_queue[i]->status = TERMINATED;
        __asm__ __volatile__("sti");
        return STATUS_OK;

      case WRITE:
        //DEBUG("[FS_TASK][COLLECT_REQUEST]: Returning new offset for: %d\n", pid);
        fd_entry_t *entry = fd_entry_table[request_queue[i]->fd];
        request_queue[i]->status = TERMINATED;
        __asm__ __volatile__("sti");
        return entry->curr_offset;
    
      case DELETE:
        __asm__ __volatile__("sti");
        return STATUS_OK;
        
      default:
        break;

      }
    }
  }

  //DEBUG("[FS_TASK][COLLECT_REQUEST]: Unable to fetch request.\n");
  __asm__ __volatile__("sti");
  return STATUS_ERROR;
}