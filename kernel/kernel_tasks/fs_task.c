#include "fs_task.h"
#include "fat32.h"
#include "sched.h"
#include "klog.h"
#include "kstring.h"
#include "kmalloc.h"
#include "blankie.h"

/*
* Author: A.H, started 27.5.2026
*/

static int request_queue_count = 0;
static int last_request_index = -1;
static fs_mailbox_queue *request_queue[MAX_TASKS];
static fd_entry_t *fd_entry_table[MAX_TASKS];
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

static void fs_wake_task(fs_mailbox_queue *req) {
  scheduler_wake_task(req->caller_pid);
}

static int fs_alloc_fd(uint32_t owner_pid, uint32_t cluster, uint32_t size) {
  DEBUG("[FS_TASK][ALLOC_FD]: Allocating new entry to fd_table\n");
  int slot = -1;
  for (int i = 0; i < MAX_TASKS; i++) {
      if (fd_entry_table[i] == NULL) {
          slot = i;
          break;
      }
  }

  if(slot == -1) {
    DEBUG("[FS_TASK][ALLOC_FD]: No free slots on the fd_table. Aborting\n");
    return STATUS_ERROR;
  }

  fd_entry_t *entry = (fd_entry_t *)kmalloc(sizeof(fd_entry_t));

  if(entry == NULL) {
    DEBUG("[FS_TASK][ALLOC_FD]: could on allocate entry. Aborting\n");
    return STATUS_ERROR;
  }

  entry->owner_pid    = owner_pid;
  entry->cluster      = cluster;
  entry->size         = size;
  entry->fd           = slot;
  entry->curr_offset  = 0;

  fd_entry_table[slot] = entry;
  DEBUG("[FS_TASK][ALLOC_FD]: Allocating successfull\n");
  return slot;
}

void fs_handle_request(fs_mailbox_queue *req) {

  if(req->request_type == OPEN) {
      uint32_t file_cluster = 0;
      uint32_t file_size = 0;
      char *path = (char *)req->path;

      if(fat32_find_file(path, &file_cluster, &file_size) != STATUS_ERROR) {
          DEBUG("[FS_TASK][handle_request]: File found succesfully, Completing request\n");
          int fd = fs_alloc_fd(req->caller_pid, file_cluster, file_size);

          if(fd == STATUS_ERROR) {
            DEBUG("[FS_TASK][handle_request]: Invalid fd number. Terminating request\n");
            req->status = TERMINATED;
            return;
          }

          req->fd     = fd;
          req->status = COMPLETE;
          return;

      }

    }
    
    DEBUG("[FS_TASK][handle_request]: Request type was invalid. Terminating request\n");
    req->status = TERMINATED;
    return;
}

/*
* 
*/
void fs_remove_from_queue() {
  
  if (last_request_index == -1 || request_queue_count == 0) {
      return;
  }

  __asm__ __volatile__("cli");

  void *ptr_to_free = (void *)request_queue[last_request_index];
  
  for (int i = last_request_index; i < request_queue_count - 1; i++) {
      request_queue[i] = request_queue[i + 1];
  }

  request_queue[request_queue_count - 1] = NULL;
  request_queue_count--;
  last_request_index = -1;

  __asm__ __volatile__("sti");

  if (ptr_to_free != NULL) {
      DEBUG("[FS_TASK][REMOVE]: Freeing request heap memory\n");
      kfree(ptr_to_free);
  }
  
  DEBUG("[FS_TASK][REMOVE]: Removing complete\n");
}

int collect_request(uint32_t pid) {
  //DEBUG("[FS_TASK][COLLECT_REQUEST]: Fetching request\n");

  for(uint32_t i = 0; i < request_queue_count; i++) {
    if(request_queue[i]->caller_pid == pid && request_queue[i]->status == COMPLETE) {
      DEBUG("[FS_TASK][COLLECT_REQUEST]: Request found!\n");
      request_queue[i]->status = TERMINATED;
      return request_queue[i]->fd;
    }
  }

  return STATUS_ERROR;
  //DEBUG("[FS_TASK][COLLECT_REQUEST]: Unable to fetch request.\n");
}

int add_request_to_queue(uint32_t pid, operations_t type, uint32_t fd, const char* path,const char *buf) {
  __asm__ __volatile__("cli");
  DEBUG("[FS_TASK][ADD_REQUEST]: adding a request for fs_task\n");

  fs_mailbox_queue *new_request = (fs_mailbox_queue *)kmalloc(sizeof(fs_mailbox_queue));

  if(new_request == NULL) {
      DEBUG("[FS_TASK][ADD_REQUEST]: could on allocate new requestat this time. Aborting\n");
      __asm__ __volatile__("sti");
      return STATUS_ERROR;
  }

  new_request->caller_pid = pid;
  new_request->request_type = type;
  new_request->fd = fd;
  
  if (path != NULL) {
      strncpy(new_request->path, path, sizeof(new_request->path) - 1);
      new_request->path[sizeof(new_request->path) - 1] = '\0';
  } else {
      new_request->path[0] = '\0';
  }
  
  if(buf != NULL) {
      strncpy(new_request->buf, buf, sizeof(new_request->buf) - 1);
      new_request->buf[sizeof(new_request->buf) - 1] = '\0';
  }
  
  DEBUG("[FS_TASK][ADD_REQUEST]: pid: %d\n", new_request->caller_pid);
  DEBUG("[FS_TASK][ADD_REQUEST]: request_type: %d\n", new_request->request_type);
  DEBUG("[FS_TASK][ADD_REQUEST]: fd: %d\n", new_request->fd);
  DEBUG("[FS_TASK][ADD_REQUEST]: buf: %s\n", new_request->buf);
  DEBUG("[FS_TASK][ADD_REQUEST]: path: %s\n", new_request->path);

  new_request->status = PENDING;
  request_queue[request_queue_count] = new_request;
  request_queue_count++;
  
  scheduler_wake_task(fs_task_pid);

  DEBUG("[FS_TASK][ADD_REQUEST]: request added\n");
  __asm__ __volatile__("sti");
  return STATUS_OK;
}

fs_mailbox_queue *find_next_request() {
  if (request_queue_count == 0) return NULL;

  for(int i = 0; i < request_queue_count; i++) {
    if(request_queue[i] != NULL && request_queue[i]->status == IN_PROGRESS) {
      last_request_index = i;
      return request_queue[i];
    }
  }

  for(int i = 0; i < request_queue_count; i++) {
    if(request_queue[i] != NULL && request_queue[i]->status == PENDING) {
      last_request_index = i;
      return request_queue[i];
    }
  }

  for(int i = 0; i < request_queue_count; i++) {
    if(request_queue[i] != NULL && request_queue[i]->status == TERMINATED) {
      last_request_index = i;
      return request_queue[i];
    }
  }

  DEBUG("[FS_TASK][NEXT_REQUEST]: could not find new request\n");
  return NULL;
}

void fs_task_loop() {
  //DEBUG("[FS_TASK]: \n");
    while(1) {
      if(request_queue_count > 0) {
        fs_mailbox_queue *request = find_next_request();
        if(request != NULL) {
          if(request->status == PENDING || request->status == IN_PROGRESS) {
            __asm__ __volatile__("cli");
            DEBUG("[FS_TASK][LOOP]: handling request\n");
            fs_handle_request(request);
            fs_wake_task(request);
            __asm__ __volatile__("sti");
          }
          
          if(request->status == TERMINATED) {
            DEBUG("[FS_TASK][LOOP]: Request is complete. Removing it\n");
            fs_remove_from_queue();
            request = NULL;
          }
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

void fs_init(task_t *fs_task) {
  mem_start   = (uint32_t)kmalloc(FS_TASK_REGION_SIZE);
  mem_end     = mem_start + FS_TASK_REGION_SIZE;
  blankie_register(fs_task_pid, fs_task->context.eip, fs_task->kernel_stack);
}
