#include "fat32.h"
#include "fs_task.h"
#include "ledger.h"

/*
 * Fs_task
 * Design & Implementation: A.H, 2026
 */

static int fs_alloc_fd(uint32_t owner_pid, uint32_t cluster, uint32_t size,
    uint32_t flags, uint32_t file_attr) {
    int slot = -1;
    for (int i = free_starting_slot; i < MAX_FD_ENTRIES; i++) {
        if (fd_entry_table[i] == NULL) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        ERROR("[FS_TASK][ALLOC_FD]: No free slots on the fd_table. Aborting\n");
        return STATUS_ERROR;
    }

    fd_entry_t *entry = (fd_entry_t *)kmalloc(sizeof(fd_entry_t));
    if (entry == NULL) {
        ERROR("[FS_TASK][ALLOC_FD]: could not allocate entry. Aborting\n");
        return STATUS_ERROR;
    }

    entry->owner_pid   = owner_pid;
    entry->cluster     = cluster;
    entry->size        = size;
    entry->fd          = slot;
    entry->curr_offset = 0;
    entry->flags       = flags;
    entry->attr        = file_attr;

    DEBUG("[FS_TASK][ALLOC_FD]: fd table created\n");
    DEBUG("[FS_TASK][ALLOC_FD]: cluster: %d\n", cluster);
    DEBUG("[FS_TASK][ALLOC_FD]: size: %d\n", size);
    DEBUG("[FS_TASK][ALLOC_FD]: fd: %d\n", slot);
    DEBUG("[FS_TASK][ALLOC_FD]: current_offset: %d\n", 0);
    DEBUG("[FS_TASK][ALLOC_FD]: flags: %d\n", flags);
    DEBUG("[FS_TASK][ALLOC_FD]: attr: %d\n", file_attr);

    fd_entry_table[slot] = entry;
    return slot;
}

static int read(request_table *req) {
    fd_entry_t *entry = fd_entry_table[req->fd];
    if (entry == NULL) {
        ERROR("[FS_TASK][READ]: entry not found\n");
        return STATUS_ERROR;
    }

    uint32_t real_buf_size = req->buffer_size;
    DEBUG("[FS_TASK][READ]: Using Buffer size: %d\n", real_buf_size);

    uint8_t *buf = (uint8_t *)kmalloc(real_buf_size);
    if (buf == NULL) {
        ERROR("[FS_TASK][READ]: buffer unallocated\n");
        return STATUS_ERROR;
    }

    if (fat32_read_file(entry->cluster, real_buf_size, buf) == STATUS_ERROR) {
        ERROR("[FS_TASK][READ]: Reading the file did not succeed\n");
        kfree(buf);
        return STATUS_ERROR;
    }

    DEBUG("[FS_TASK][READ]: Capping buffer at index: %d\n", entry->size);
    buf[entry->size] = '\0';
    DEBUG("[FS_TASK][READ]: memcpy to req.buf with size: %d\n", entry->size);
    memcpy(req->buf, (char *)buf, entry->size);
    kfree(buf);
    return STATUS_OK;
}

static int open(request_table *req) {
    uint32_t file_cluster = 0;
    uint32_t file_size    = 0;
    uint8_t file_attr     = 0;
    const char *path      = (char *)req->path;
    char filename[8];

    if (fat32_find_cluster(path, &file_cluster, &file_size, filename, &file_attr) ==
        STATUS_ERROR) {
        ERROR("[FS_TASK][OPEN]: Could not find file.\n");
        return STATUS_ERROR;
    }

    int fd = fs_alloc_fd(req->caller_pid, file_cluster, file_size, req->flags, file_attr);
    if (fd == STATUS_ERROR) {
        ERROR("[FS_TASK][OPEN]: Invalid fd number. Terminating request\n");
        return STATUS_ERROR;
    }

    req->fd = fd;
    memcpy(req->buf, filename, 8);
    return STATUS_OK;
}

static int create(request_table *req) {
    DEBUG("[FS_TASK][CREATE]: Creating a new directory for %s\n", req->path);

    uint32_t base_directory = f32_fs.root_cluster;

    if (req->buffer_size > 8) {
        if (fat32_mkdirp(base_directory, req->path) == STATUS_ERROR) {
            ERROR("[FS_TASK][CREATE]: mkdirp failed!\n");
            return STATUS_ERROR;
        }
    } else {
        if (fat32_mkdir(base_directory, req->path) == STATUS_ERROR) {
            ERROR("[FS_TASK][CREATE]: mkdir failed!\n");
            return STATUS_ERROR;
        }
    }

    fat32_list_dir(base_directory);
    return STATUS_OK;
}

static int write(request_table *req) {
    fd_entry_t *entry = fd_entry_table[req->fd];
    if (entry == NULL) {
        ERROR("[FS_TASK][WRITE]: entry not found\n");
        return STATUS_ERROR;
    }

    if (!(entry->flags & O_WRONLY) && !(entry->flags & O_RDWR)) {
        ERROR("[FS_TASK][WRITE]: flag mismatch\n");
        return STATUS_ERROR;
    }

    if (fat32_write_file_at_offset(entry->cluster, entry->curr_offset,
            req->buf, req->buffer_size) == STATUS_ERROR) {
        ERROR("[FS_TASK][WRITE]: Could not write to opened file\n");
        return STATUS_ERROR;
    }

    DEBUG("[FS_TASK][WRITE]: Write was successful\n");
    entry->curr_offset = req->buffer_size;
    entry->size        = req->buffer_size;

    if (fat32_update_dirent_size(f32_fs.root_cluster, entry->cluster,
            entry->size) == STATUS_ERROR) {
        ERROR("[FS_TASK][WRITE]: Could not update file\n");
        return STATUS_ERROR;
    }

    DEBUG("[FS_TASK][WRITE]: current offset %d and size %d\n",
        entry->curr_offset, entry->size);
    return STATUS_OK;
}

static int close(request_table *req) {
    fd_entry_t *entry = fd_entry_table[req->fd];
    if (entry == NULL) {
        ERROR("[FS_TASK][CLOSE]: entry not found\n");
        return STATUS_ERROR;
    }

    DEBUG("[FS_TASK][CLOSE]: Closing fd: %d\n", entry->fd);
    fd_entry_table[req->fd] = NULL;
    kfree(entry);
    return STATUS_OK;
}

void fs_handle_request(request_table *req) {
    task_t *fs_task = task_get(fs_task_pid);

    if (fs_task == NULL || req == NULL) {
        return;
    }

    switch (req->request_type) {
    case OPEN:
        req->status       = (open(req) == STATUS_ERROR) ? TERMINATED : COMPLETE;
        fs_task->priority = PRIORITY_LOW;
        fs_wake_task(req->caller_pid);
        return;
    case READ:
        req->status       = (read(req) == STATUS_ERROR) ? TERMINATED : COMPLETE;
        fs_task->priority = PRIORITY_LOW;
        fs_wake_task(req->caller_pid);
        return;
    case WRITE:
        req->status       = (write(req) == STATUS_ERROR) ? TERMINATED : COMPLETE;
        fs_task->priority = PRIORITY_LOW;
        fs_wake_task(req->caller_pid);
        return;
    case CLOSE:
        req->status = (close(req) == STATUS_ERROR) ? FAILED : TERMINATED;
        return;
    case CREATE:
        req->status       = (create(req) == STATUS_ERROR) ? FAILED : TERMINATED;
        fs_task->priority = PRIORITY_LOW;
        fs_wake_task(req->caller_pid);
        return;
    default:
        ERROR("[FS_TASK][HANDLE_REQUEST]: invalid request type\n");
        req->status = TERMINATED;
        fs_wake_task(req->caller_pid);
        return;
    }
}