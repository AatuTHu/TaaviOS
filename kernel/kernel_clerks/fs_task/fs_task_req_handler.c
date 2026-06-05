#include "fs_task.h"
#include "fat32.h"

/*
* Fs_task
* Design & Implementation: A.H, 2026
*/

/*
* This source file contains the logic to handle the selected request.
* Here are functions wake_task, alloc_fd, read, open, close, write, delete, handle request
* 
* I have tried to implement a way in which we go the shortest safe route to achieving results and can wake the caller
* 
*/

//signal scheduler to wake the pid who made the request.
static void fs_wake_task(uint32_t pid) {
  scheduler_wake_task(pid);
}

//If opening a file is succesfull we come here and make an entry to fd_table which is separate table from the request table.
// there can be multiple request with the same fd number but it alwasys correspond to the fd given here once a succesfully file is opened
static int fs_alloc_fd(uint32_t owner_pid, uint32_t cluster, uint32_t size, const char *flags) {
  DEBUG("[FS_TASK][ALLOC_FD]: Allocating new entry to fd_table\n");
  int slot = -1;
  for (int i = free_starting_slot; i < MAX_TASKS; i++) { //free_starting_slot is 3. We reserve them for standtart i/o.
      if (fd_entry_table[i] == NULL) {
          slot = i;
          break;
      }
  }

  if(slot == -1) {
    ERROR("[FS_TASK][ALLOC_FD]: No free slots on the fd_table. Aborting\n");
    return STATUS_ERROR;
  }

  fd_entry_t *entry = (fd_entry_t *)kmalloc(sizeof(fd_entry_t));

  if(entry == NULL) {
    ERROR("[FS_TASK][ALLOC_FD]: could on allocate entry. Aborting\n");
    return STATUS_ERROR;
  }

  entry->owner_pid    = owner_pid;
  entry->cluster      = cluster;
  entry->size         = size;
  entry->fd           = slot;
  entry->curr_offset  = 0;
  strncpy(entry->flags, flags, sizeof(entry->flags));
  

  fd_entry_table[slot] = entry;
  DEBUG("[FS_TASK][ALLOC_FD]: Allocating successfull fd: %d\n", slot);
  return slot;
}

/*
* The read file handler
*/
static int read(fs_mailbox_queue *req) {
      if(req->fd >= MAX_TASKS) {
        ERROR("[FS_TASK][handle_request]: fd number invalid\n");
        return STATUS_ERROR;
      }

      const fd_entry_t *entry = fd_entry_table[req->fd];

      if(entry == NULL) {
        ERROR("[FS_TASK][handle_request]: entry not found\n");
        return STATUS_ERROR;
      }

      uint32_t real_buf_size = req->buffer_size;
      
      //If the real buff_size is biger than the size of opened file change it to actual size
      if(real_buf_size > entry->size) { //On my test case this is 512 > 17 so it always uses 17
        real_buf_size = entry->size;
      }

      uint8_t *buf = (uint8_t *)kmalloc(real_buf_size);

      if(buf == NULL) {
        ERROR("[FS_TASK][handle_request]: buffer unallocated\n");
        return STATUS_ERROR;
      }

      
      if(fat32_read_file(entry->cluster, real_buf_size, buf) == STATUS_ERROR) {
        ERROR("[FS_TASK][handle_request]: Reading the file did not succeed\n");
        kfree(buf);
        return STATUS_ERROR;
      } 

      //save the real size to request buffer size so it an be used when task is collecting
      req->buffer_size = real_buf_size;    
      // cape the files end at the real_buf_size.
      buf[real_buf_size] = '\0'; 
      //Copy the opened file to request buf field
      memcpy(req->buf, (char*)buf, real_buf_size); 
      kfree(buf);

      DEBUG("[FS_TASK][handle_request]: Marking request with owner pid %d complete\n", req->caller_pid);
      return STATUS_OK;
}

// A simple open file. Flags coming soon
static int open(fs_mailbox_queue *req) {
      uint32_t file_cluster = 0;
      uint32_t file_size = 0;
      const char *path = (char *)req->path;

      if(fat32_find_file(path, &file_cluster, &file_size) == STATUS_ERROR) {
        ERROR("[FS_TASK][handle_request]: Could not find file.\n");
        return STATUS_ERROR;
      }

      DEBUG("[FS_TASK][handle_request]: File found succesfully, Completing request\n");
      int fd = fs_alloc_fd(req->caller_pid, file_cluster, file_size, req->flags);

      if(fd == STATUS_ERROR) {
        ERROR("[FS_TASK][handle_request]: Invalid fd number. Terminating request\n");
        return STATUS_ERROR;
      }

      req->fd = fd;
      return STATUS_OK;
    }

//entry point of this file. We come here from loop and then decide what to do next
void fs_handle_request(fs_mailbox_queue *req) {
  //get a pointer for fs_task so we can cgange its states later
  task_t *fs_task = task_get(fs_task_pid);

  if(fs_task == NULL || req == NULL) { 
    ERROR("[FS_TASK][HANDLE_REQUST]: Couldn find pointer of self\n");
    return;
  }

  if(req->request_type == OPEN) {
     //__asm__ __volatile__("cli");
     if(open(req) == STATUS_ERROR) {
      req->status = TERMINATED;
     } else {
      req->status = COMPLETE;
     }
    fs_task->priority = PRIORITY_LOW;
    fs_wake_task(req->caller_pid);
    //__asm__ __volatile__("sti");
    return;
  }

  if(req->request_type == READ) {
    //__asm__ __volatile__("cli");
    if(read(req) == STATUS_ERROR) {
      req->status = TERMINATED;
    } else {
      req->status = COMPLETE;
    }
    task_t *fs_task = task_get(fs_task_pid);
    fs_task->priority = PRIORITY_LOW;
    fs_wake_task(req->caller_pid);
    //__asm__ __volatile__("sti");
    return;
  }
    
    
    ERROR("[FS_TASK][handle_request]: Request type was invalid. Terminating request\n");
    req->status = TERMINATED;
    fs_wake_task(req->caller_pid);
    return;
}