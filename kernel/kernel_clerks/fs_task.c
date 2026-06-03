#include "fs_task.h"
#include "fat32.h"
#include "sched.h"
#include "klog.h"
#include "kstring.h"
#include "kmalloc.h"
#include "blankie.h"
#include "ledger.h"

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

#define free_starting_slot 3
static int request_queue_count = 0;
static fs_mailbox_queue *request_queue[MAX_TASKS];
static fd_entry_t *fd_entry_table[MAX_TASKS];

static void fs_wake_task(uint32_t pid) {
  scheduler_wake_task(pid);
}

static int fs_alloc_fd(uint32_t owner_pid, uint32_t cluster, uint32_t size) {
  DEBUG("[FS_TASK][ALLOC_FD]: Allocating new entry to fd_table\n");
  int slot = -1;
  for (int i = free_starting_slot; i < MAX_TASKS; i++) {
      if (fd_entry_table[i] == NULL) {
          slot = i;
          break;
      }
  }

  if(slot == -1) {
    DEBUG("[FS_TASK][ALLOC_FD]: No free slots on the fd_table. Aborting\n");
    return STATUS_ERROR;
  }

  uint32_t protocol_addr = ledger_alloc(fs_task_pid, sizeof(fd_entry_t));

  if(protocol_addr == 0) {
    DEBUG("[FS_TASK][ALLOC_FD]: could on allocate entry. Aborting\n");
    return STATUS_ERROR;
  }

  fd_entry_t *entry = (fd_entry_t *)ledger_validate(fs_task_pid, protocol_addr);


  entry->owner_pid    = owner_pid;
  entry->cluster      = cluster;
  entry->size         = size;
  entry->fd           = slot;
  entry->curr_offset  = 0;

  fd_entry_table[slot] = (fd_entry_t *)protocol_addr;
  DEBUG("[FS_TASK][ALLOC_FD]: Allocating successfull\n");
  return slot;
}

static void fs_handle_request(fs_mailbox_queue *req) {

  if(req->request_type == OPEN) {
      uint32_t file_cluster = 0;
      uint32_t file_size = 0;
      const char *path = (char *)req->path;

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
          fs_wake_task(req->caller_pid);
          return;

      }

    }

    if(req->request_type == READ) {
      if(req->fd >= MAX_TASKS) {
        DEBUG("[FS_TASK][handle_request]: fd number invalid\n");
        return;
      }

      const fd_entry_t *entry = (fd_entry_t *)ledger_validate(fs_task_pid, (uint32_t)fd_entry_table[req->fd]);

      if(entry == 0) {
        DEBUG("[FS_TASK][handle_request]: entry not found\n");
        req->status = TERMINATED;
        return;
      }

      uint32_t buf_size = fat32_calculate_cluster_size();
      uint8_t *buf = (uint8_t *)kmalloc(buf_size);

      if(buf == NULL) {
        DEBUG("[FS_TASK][handle_request]: buffer unallocated\n");
        req->status = TERMINATED;
        return;
      }

      if(fat32_read_file(entry->cluster, entry->size, buf) == STATUS_ERROR) {
        DEBUG("[FS_TASK][handle_request]: Reading the file did not succeed\n");
        kfree(buf);
        req->status = TERMINATED;
        return;
      } 
      
      DEBUG("[FS_TASK][handle_request]: file read succesfully: %s", buf);
      buf[entry->size] = '\0';
      memcpy(req->buf, (char*)buf, buf_size-1);
      
      kfree(buf);
      DEBUG("[FS_TASK][handle_request]: Marking request with owner pid %d complete\n", req->caller_pid);
      req->status = COMPLETE;
      fs_wake_task(req->caller_pid);
      return;
    }
    
    
    DEBUG("[FS_TASK][handle_request]: Request type was invalid. Terminating request\n");
    req->status = TERMINATED;
    return;
}

/*
* 
*/
static void fs_remove_from_queue(const fs_mailbox_queue *req) {
  
  if (request_queue_count == 0 || req == NULL) {
      return;
  }

  for (int i = 0; i < request_queue_count - 1; i++) {
    request_queue[i] = request_queue[i + 1];
  }
  
  request_queue[request_queue_count - 1] = NULL;
  request_queue_count--;
  
  DEBUG("[FS_TASK][REMOVE]: Freeing request heap memory\n");
  ledger_free(fs_task_pid, (uint32_t)req);
  DEBUG("[FS_TASK][REMOVE]: Removing complete\n");
  
}


/*
* This function gives the result to outside world. Mainly for the caller in syscall
*/
int collect_request(uint32_t pid, char *out) {
  //DEBUG("[FS_TASK][COLLECT_REQUEST]: Fetching request\n");

  
  for(int i = 0; i < request_queue_count; i++) {
    fs_mailbox_queue *req = (fs_mailbox_queue *)ledger_validate(fs_task_pid, (uint32_t)request_queue[i]);

    if(req == 0) {
      DEBUG("[FS_TASK][COLLECT_REQUEST]: Invalid address\n");
      continue;
    }
    

    if(req->caller_pid == pid && req->status == COMPLETE) {
      DEBUG("[FS_TASK][COLLECT_REQUEST]: Request found!\n");
      req->status = TERMINATED;
      if(req->request_type == OPEN) return req->fd;
      if(req->request_type == READ) {
        uint32_t buf_size = fat32_calculate_cluster_size();
        memcpy(out, req->buf, buf_size-1);
        DEBUG("[FS_TASK][COLLECT_REQUEST]: copied size: %d\n", buf_size-1);
        DEBUG("[FS_TASK][COLLECT_REQUEST]: buffer content %s", out);
        return STATUS_OK;
      }
    }
  }

  return STATUS_ERROR;
  //DEBUG("[FS_TASK][COLLECT_REQUEST]: Unable to fetch request.\n");
}

/*
* ADD_REQUEST_TO_QUEUE
* Takes the params sent to it via syscall and makes an work "order" for fs_task. 
*/
int add_request_to_queue(uint32_t pid, operations_t type, uint32_t fd, const char* path, const char *buf) {
  __asm__ __volatile__("cli");
  DEBUG("[FS_TASK][ADD_REQUEST]: adding a request for fs_task\n");

  uint32_t protocol_addr = ledger_alloc(fs_task_pid, sizeof(fs_mailbox_queue));

  if(protocol_addr == 0) {
      DEBUG("[FS_TASK][ADD_REQUEST]: could on allocate new requestat this time. Aborting\n");
      __asm__ __volatile__("sti");
      return STATUS_ERROR;
  }

  fs_mailbox_queue *new_request =  (fs_mailbox_queue *)ledger_validate(fs_task_pid, protocol_addr);

  new_request->caller_pid = pid;
  new_request->request_type = type;
  new_request->fd = fd;
  
  if (path != NULL) {
      strncpy(new_request->path, path, sizeof(new_request->path) - 1);
      new_request->path[sizeof(new_request->path) - 1] = '\0';
  } else {
      new_request->path[0] = '\0';
  }
  
  if(buf != NULL && type == WRITE) {
      strncpy(new_request->buf, buf, sizeof(new_request->buf) - 1);
      new_request->buf[sizeof(new_request->buf) - 1] = '\0';
      DEBUG("[FS_TASK][ADD_REQUEST]: buf: %s\n", new_request->buf);
  }
  
  DEBUG("[FS_TASK][ADD_REQUEST]: pid: %d\n", new_request->caller_pid);
  DEBUG("[FS_TASK][ADD_REQUEST]: request_type: %d\n", new_request->request_type);
  DEBUG("[FS_TASK][ADD_REQUEST]: fd: %d\n", new_request->fd);
  DEBUG("[FS_TASK][ADD_REQUEST]: path: %s\n", new_request->path);

  new_request->status = PENDING;
  request_queue[request_queue_count] = (fs_mailbox_queue *)protocol_addr;
  request_queue_count++;
  
  DEBUG("[FS_TASK][ADD_REQUEST]: request added\n");
  scheduler_wake_task(fs_task_pid);
  
  __asm__ __volatile__("sti");
  return STATUS_OK;
}

/*
* This functions was inspired from schedulers next task find function.
* 
*/
static fs_mailbox_queue *find_next_request() {
  if (request_queue_count == 0) return NULL;

  for(int i = 0; i < request_queue_count; i++) {
    fs_mailbox_queue *req = (fs_mailbox_queue *)ledger_validate(fs_task_pid, (uint32_t)request_queue[i]);
    if(req != 0 && req->status == IN_PROGRESS) {
      return req;
    }
  }

  for(int i = 0; i < request_queue_count; i++) {
    fs_mailbox_queue *req = (fs_mailbox_queue *)ledger_validate(fs_task_pid, (uint32_t)request_queue[i]);
    if(req != 0 && req->status == PENDING) {
      return req;
    }
  }

  for(int i = 0; i < request_queue_count; i++) {
    fs_mailbox_queue *req = (fs_mailbox_queue *)ledger_validate(fs_task_pid, (uint32_t)request_queue[i]);
    if(req != 0 && req->status == TERMINATED) {
      return req;
    }
  }

  request_queue_count = 0;

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
            __asm__ __volatile__("cli");
            DEBUG("[FS_TASK][LOOP]: handling request\n");
            fs_handle_request(request);
            __asm__ __volatile__("sti");
          }
          
          if(request->status == TERMINATED) {
            __asm__ __volatile__("cli");
            DEBUG("[FS_TASK][LOOP]: Request is complete. Removing it\n");
            fs_remove_from_queue(request);
            request = NULL;
             __asm__ __volatile__("sti");
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


void fs_init(const task_t *fs_task) {
  ledger_register(fs_task_pid);
  blankie_register(fs_task_pid, fs_task->context.eip, fs_task->kernel_stack);
}
