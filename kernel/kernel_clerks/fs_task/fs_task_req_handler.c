#include "fs_task.h"
#include "fat32.h"

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

void fs_handle_request(fs_mailbox_queue *req) {

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
      
      uint8_t *buf = (uint8_t *)kmalloc(req->buffer_size-1);

      if(buf == NULL) {
        DEBUG("[FS_TASK][handle_request]: buffer unallocated\n");
        req->status = TERMINATED;
        return;
      }

      if(fat32_read_file(entry->cluster, req->buffer_size-1, buf) == STATUS_ERROR) {
        DEBUG("[FS_TASK][handle_request]: Reading the file did not succeed\n");
        kfree(buf);
        req->status = TERMINATED;
        return;
      } 
      
      DEBUG("[FS_TASK][handle_request]: file read succesfully: %s", buf);
      buf[entry->size] = '\0';
      memcpy(req->buf, (char*)buf, entry->size);
      
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