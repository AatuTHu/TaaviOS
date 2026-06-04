#include "fs_task.h"


/*
* ADD_REQUEST_TO_QUEUE
* Takes the params sent to it via syscall and makes an work "order" for fs_task. 
*/
int add_request_to_queue(uint32_t pid, operations_t type, uint32_t fd, const char* path, const char *buf, uint32_t buffer_size) {
  DEBUG("[FS_TASK][ADD_REQUEST]: adding a request for fs_task\n");

  fs_mailbox_queue *new_request = (fs_mailbox_queue*)kmalloc(sizeof(fs_mailbox_queue));

  if(new_request == NULL) {
     // DEBUG("[FS_TASK][ADD_REQUEST]: could on allocate new requestat this time. Aborting\n");
      scheduler_wake_task(pid);
      return STATUS_ERROR;
  }

  new_request->caller_pid = pid;
  new_request->request_type = type;
  new_request->fd = fd;
  new_request->buffer_size = buffer_size;
  
  if (path != NULL) {
      strncpy(new_request->path, path, sizeof(new_request->path) - 1);
      new_request->path[sizeof(new_request->path) - 1] = '\0';
  } else {
      new_request->path[0] = '\0';
  }
  
  if(buf != NULL && type == WRITE) {
      strncpy(new_request->buf, buf, sizeof(new_request->buf) - 1);
      new_request->buf[sizeof(new_request->buf) - 1] = '\0';
   //   DEBUG("[FS_TASK][ADD_REQUEST]: buf: %s\n", new_request->buf);
  }
  
 // DEBUG("[FS_TASK][ADD_REQUEST]: pid: %d\n", new_request->caller_pid);
 // DEBUG("[FS_TASK][ADD_REQUEST]: request_type: %d\n", new_request->request_type);
 // DEBUG("[FS_TASK][ADD_REQUEST]: fd: %d\n", new_request->fd);
 // DEBUG("[FS_TASK][ADD_REQUEST]: path: %s\n", new_request->path);
 // DEBUG("[FS_TASK][ADD_REQUEST]: buffer_size: %d\n", new_request->buffer_size);

  new_request->status = PENDING;
  request_queue[request_queue_count] = new_request;
  request_queue_count++;
  
  DEBUG("[FS_TASK][ADD_REQUEST]: request added\n");
  task_t *fs_task = task_get(fs_task_pid);
  
  fs_task->priority = PRIORITY_HIGH;
  fs_task->state    = TASK_READY;

  return STATUS_OK;
}

/*
* This function gives the result to outside world. Mainly for the caller in syscall
*/
int collect_request(uint32_t pid, char *out) {
  //DEBUG("[FS_TASK][COLLECT_REQUEST]: Fetching request\n");

  
  for(int i = 0; i < request_queue_count; i++) {
    if(request_queue[i] != NULL && request_queue[i]->caller_pid == pid && request_queue[i]->status == COMPLETE) {
      DEBUG("[FS_TASK][COLLECT_REQUEST]: Request found!\n");

      request_queue[i]->status = TERMINATED;

      if(request_queue[i]->request_type == OPEN) return request_queue[i]->fd;

      if(request_queue[i]->request_type == READ) {
        DEBUG("[FS_TASK][COLLECT_REQUEST]: Request was READ!\n");
        fd_entry_t *entry = fd_entry_table[request_queue[i]->fd];
        memcpy(out, request_queue[i]->buf, entry->size);
        DEBUG("[FS_TASK][COLLECT_REQUEST]: copied size: %d\n", entry->size);
        DEBUG("[FS_TASK][COLLECT_REQUEST]: buffer content %s", out);
        return STATUS_OK;
      }

    }
  }

  return STATUS_ERROR;
  //DEBUG("[FS_TASK][COLLECT_REQUEST]: Unable to fetch request.\n");
}