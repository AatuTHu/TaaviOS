#include "fs_task.h"
#include "fat32.h"
#include "sched.h"
#include "klog.h"
#include "kstring.h"
#include "kmalloc.h"
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

#define free_starting_slot 3
static int request_queue_count = 0;
static int current_req_index = -1;
static fs_mailbox_queue *request_queue[MAX_TASKS];
static fd_entry_t *fd_entry_table[MAX_TASKS];

void fs_recovery() {
  DEBUG("[FS_TASK][RECOVERY]: PROTOCOL HAIL MARY LAUNCHED\n");
  if(current_req_index == -1) {
    DEBUG("[FS_TASK][RECOVERY]: No freeing needed\n");
    return;
  }
  fs_mailbox_queue *req = request_queue[current_req_index];
  if(req == NULL) return;
  kfree(req);
  blankie_activate(fs_task_pid);
}

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

static void fs_handle_request(fs_mailbox_queue *req) {

  if(req->request_type == OPEN) {
      uint32_t file_cluster = 0;
      uint32_t file_size = 0;
      const char *path = (char *)req->path;

      if(fat32_find_file(path, &file_cluster, &file_size) == STATUS_ERROR) {
        DEBUG("[FS_TASK][handle_request]: Could not find file.\n");
        req->status = TERMINATED;
        return;
      }

      DEBUG("[FS_TASK][handle_request]: File found succesfully, Completing request\n");
      int fd = fs_alloc_fd(req->caller_pid, file_cluster, file_size);

      if(fd == STATUS_ERROR) {
        DEBUG("[FS_TASK][handle_request]: Invalid fd number. Terminating request\n");
        req->status = TERMINATED;
        return;
      }

      req->fd     = fd;
      req->status = COMPLETE;
      task_t *fs_task = task_get(fs_task_pid);
      fs_task->priority = PRIORITY_LOW;
      fs_wake_task(req->caller_pid);
      return;

    }

    if(req->request_type == READ) {
      if(req->fd >= MAX_TASKS) {
        DEBUG("[FS_TASK][handle_request]: fd number invalid\n");
        return;
      }

      const fd_entry_t *entry = fd_entry_table[req->fd];

      if(entry == NULL) {
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

      if(fat32_read_file(entry->cluster, buf_size, buf) == STATUS_ERROR) {
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
      task_t *fs_task = task_get(fs_task_pid);
      req->status = COMPLETE;
      fs_task->priority = PRIORITY_LOW;
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
static void fs_remove_from_queue() {

  fs_mailbox_queue *req = NULL;

  for(int i = 0; i < request_queue_count; i++) {
    if(request_queue[i] != NULL && request_queue[i]->status == TERMINATED) {
      req = request_queue[i];
    }
  }
  
  if (request_queue_count == 0 || req == NULL) {
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
        uint32_t buf_size = fat32_calculate_cluster_size();
        memcpy(out, request_queue[i]->buf, buf_size-1);
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

  fs_mailbox_queue *new_request = (fs_mailbox_queue*)kmalloc(sizeof(fs_mailbox_queue));

  if(new_request == NULL) {
      DEBUG("[FS_TASK][ADD_REQUEST]: could on allocate new requestat this time. Aborting\n");
      scheduler_wake_task(pid);
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
  request_queue[request_queue_count] = new_request;
  request_queue_count++;
  
  DEBUG("[FS_TASK][ADD_REQUEST]: request added\n");
  task_t *fs_task = task_get(fs_task_pid);
  
  fs_task->priority = PRIORITY_HIGH;
  fs_task->state    = TASK_READY;
  
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

  request_queue_count = 0;

  //DEBUG("[FS_TASK][NEXT_REQUEST]: could not find new request\n");
  return NULL;
}


/*
* Main task loop of fs_task the algorithm
*/
/*
* Main task loop of fs_task the algorithm
*/
void fs_task_loop() {
  //DEBUG("[FS_TASK]: \n");
    while(1) {
    if(request_queue_count > 0) {
        fs_mailbox_queue *request = find_next_request();
        if(request != NULL && request->status != TERMINATED) {
            request->status = IN_PROGRESS;
            __asm__ __volatile__("cli");
            DEBUG("[FS_TASK][LOOP]: handling request\n");
            fs_handle_request(request);
            __asm__ __volatile__("sti");
            current_req_index = -1;
        }
    }

    fs_remove_from_queue();

    if(request_queue_count == 0) {
        current_req_index = -1;
        blankie_activate(fs_task_pid);
    }
  }
}


void fs_init(const task_t *fs_task) {
  register_hail_mary_function(fs_task_pid, fs_recovery);
  blankie_register(fs_task_pid, fs_task->context.eip, fs_task->kernel_stack);
}
