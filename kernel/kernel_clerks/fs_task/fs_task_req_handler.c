#include "fat32.h"
#include "fs_task.h"

static dir_traversal_t *dir_map[MAX_TASKS];
static tasks_dir_t virt_tasks_dir[MAX_TASKS];
/*
 * Fs_task
 * Design & Implementation: A.H, 2026
 */

/**
 * dir_traversal_mapper - creates an entry of the current and previous directory clusters.
 * @param owner_pid: short description.
 * @param current_cluster: short description.
 * @param prev_cluster: short description.
 *
 * Description:
 * This function creates an entry of tasks current and previous directories, so that directory
 * traversal dosn't have to go trough fat32. If there already is an existing entry for the owner pid
 * the existing is wiped and the new one will be made in its place
 * thus allowing a task to only ever have one directory traversal map entry
 *
 * Return: STATUS_OK || STATUS_ERROR
 */
static int dir_traversal_mapper(uint32_t owner_pid, uint32_t current_cluster, uint32_t prev_cluster) {
    int slot = -1;

    for (int i = 0; i < MAX_TASKS; i++) {
        if (dir_map[i] != NULL && dir_map[i]->owner_pid == owner_pid) {
            DEBUG_FS_TASK("[FS_TASK][DIR_MAPPER]: entry found with slot: %d\n", i);
            kfree(dir_map[i]);
            dir_map[i] = NULL;
            slot       = i;
        }
    }

    if (slot == -1) {
        for (int i = 0; i < MAX_TASKS; i++) {
            if (dir_map[i] == NULL) {
                slot = i;
                break;
            }
        }
    }

    if (slot == -1) {
        DEBUG_FS_TASK("[FS_TASK][DIR_MAPPER]: No free slots in the map\n");
        return STATUS_ERROR;
    }

    dir_traversal_t *entry = (dir_traversal_t *)kmalloc(sizeof(dir_traversal_t));

    if (entry == NULL) {
        DEBUG_FS_TASK("[FS_TASK][DIR_MAPPER]: Heap allocation failed\n");
        return STATUS_ERROR;
    }

    if (prev_cluster <= 2) {
        prev_cluster = f32_fs.root_cluster;
    }

    entry->owner_pid       = owner_pid;
    entry->current_cluster = current_cluster;
    entry->prev_cluster    = prev_cluster;

    DEBUG_FS_TASK("[FS_TASK][MAPPER]: owner_pid: %d\n", owner_pid);
    DEBUG_FS_TASK("[FS_TASK][MAPPER]: current_cluster: %d\n", current_cluster);
    DEBUG_FS_TASK("[FS_TASK][MAPPER]: prev_cluster: %d\n", prev_cluster);

    dir_map[slot] = entry;

    return STATUS_OK;
}

/**
 * dir_get_direction - Fetches dir traversal entry.
 * @param owner_pid: short description.
 *
 * Description:
 * Function searches for the dir traversal entry info of given owner_pid param.
 *
 * Context: It is beneficial to what is the current working directory to navigate clusters more easily.
 * Return: pointer to an dir_map entry || NULL
 */
static dir_traversal_t *dir_get_direction(uint32_t owner_pid) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (dir_map[i] != NULL && dir_map[i]->owner_pid == owner_pid) {
            DEBUG_FS_TASK("[FS_TASK][GET_DIRECTION]: directions found!\n");
            DEBUG_FS_TASK("[FS_TASK][GET_DIRECTION]: Current cluster %d\n", dir_map[i]->current_cluster);
            DEBUG_FS_TASK("[FS_TASK][GET_DIRECTION]: Prev_cluster %d\n", dir_map[i]->prev_cluster);
            return dir_map[i];
        }
    }
    DEBUG_FS_TASK("[FS_TASK][GET_DIRECTION]: directions not found for task pid: %d!\n", owner_pid);
    return NULL;
}

/**
 * fs_alloc_fd - Creates an fd table entry of opened file.
 * @param owner_pid: short description.
 * @param file_cluster: short description.
 * @param dir_cluster: short description.
 * @param size: short description.
 * @param flags: short description.
 * @param file_attr: short description.
 *
 * Description:
 * This function creates an entry of an opened file, filling it with metadata of the file.
 *
 * Return: index of the entry || STATUS_ERROR
 */
static int fs_alloc_fd(uint32_t owner_pid, uint32_t file_cluster, uint32_t dir_cluster, uint32_t size,
                       uint32_t flags, uint8_t file_attr) {
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

    entry->owner_pid    = owner_pid;
    entry->file_cluster = file_cluster;
    entry->dir_cluster  = dir_cluster;
    entry->size         = size;
    entry->fd           = slot;
    entry->curr_offset  = 0;
    entry->flags        = flags;
    entry->attr         = file_attr;

    DEBUG_FS_TASK("[FS_TASK][ALLOC_FD]: fd table created\n");
    DEBUG_FS_TASK("[FS_TASK][ALLOC_FD]: file_cluster: %d\n", file_cluster);
    DEBUG_FS_TASK("[FS_TASK][ALLOC_FD]: dir_cluster: %d\n", dir_cluster);
    DEBUG_FS_TASK("[FS_TASK][ALLOC_FD]: size: %d\n", size);
    DEBUG_FS_TASK("[FS_TASK][ALLOC_FD]: fd: %d\n", slot);
    DEBUG_FS_TASK("[FS_TASK][ALLOC_FD]: current_offset: %d\n", 0);
    DEBUG_FS_TASK("[FS_TASK][ALLOC_FD]: flags: %d\n", flags);
    DEBUG_FS_TASK("[FS_TASK][ALLOC_FD]: attr: %d\n", file_attr);

    fd_entry_table[slot] = entry;
    return slot;
}

/**
 * read - Reads opened file.
 * @param req: pointer to an request_table entry
 *
 * Description:
 * This file reads to a local buffer the contents of a file that is opened.
 * should a file not be opened or allocating a local buffer fail the function aborts the request.
 * if the read is succesfull the input stream is capped at the read size with '\0' and then
 * copied to request_table req->buf buffer. Then the actual read size is saved to req-> buffer_size.
 *
 * Return: STATUS_OK || STATUS_ERROR
 */
static int read(request_table *req) {

    /*
     * Dont fucking touch this function ever again.
     */

    fd_entry_t *entry = fd_entry_table[req->fd];
    if (entry == NULL) {
        ERROR("[FS_TASK][READ]: entry not found\n");
        return STATUS_ERROR;
    }

    uint8_t *buf = (uint8_t *)kmalloc(entry->size + 1);
    if (buf == NULL) {
        ERROR("[FS_TASK][READ]: buffer unallocated\n");
        return STATUS_ERROR;
    }

    if (fat32_read_file(entry->file_cluster, entry->size, buf) == STATUS_ERROR) {
        ERROR("[FS_TASK][READ]: Reading the file did not succeed\n");
        kfree(buf);
        return STATUS_ERROR;
    }

    DEBUG_FS_TASK("[FS_TASK][READ]: Capping buffer at index: %d\n", entry->size);
    buf[entry->size] = '\0';
    DEBUG_FS_TASK("[FS_TASK][READ]: memcpy to req.buf with size: %d\n", entry->size);
    memcpy(req->buf, (char *)buf, entry->size);
    req->buffer_size = entry->size;
    kfree(buf);
    return STATUS_OK;
}

/**
 * open - Opens a file at the end of the given path.
 * @param req: pointer to an request_table entry
 *
 * Description:
 * This function opens a file at the end of the given path that comes from request_table req->path field.
 * before opening the file it tries to check if there is a directory_tarversal entry for the requester.
 * If there is the starting cluster will be the one found from there. Otherwise it will use the root cluster.
 * If the file is found the function creates an fd entry of it and places the fd number to req->fd field.
 *
 * Context: Why was it made, when to call it.
 * Return: STATUS_OK || STATUS_ERROR
 */
static int open(request_table *req) {
    uint32_t file_cluster     = 0;
    uint32_t dir_cluster      = 0;
    uint32_t file_size        = 0;
    uint8_t file_attr         = 0;
    uint32_t starting_cluster = f32_fs.root_cluster;
    char filename[TASK_NAME_LENGTH];
    const char *path     = (char *)req->path;

    dir_traversal_t *map = dir_get_direction(req->caller_pid);

    if (map != NULL) {
        starting_cluster = map->current_cluster;
    }

    if (fat32_find_cluster(starting_cluster, path, &file_cluster, &dir_cluster, &file_size, filename, &file_attr) ==
        STATUS_ERROR) {
        ERROR("[FS_TASK][OPEN]: Could not find file.\n");
        return STATUS_ERROR;
    }

    int fd = fs_alloc_fd(req->caller_pid, file_cluster, dir_cluster, file_size, req->flags, file_attr);
    if (fd == STATUS_ERROR) {
        ERROR("[FS_TASK][OPEN]: Invalid fd number. Terminating request\n");
        return STATUS_ERROR;
    }

    req->fd = fd;
    return STATUS_OK;
}

/**
 * create - Creates a directory entry.
 * @param req: pointer to an request_table entry
 *
 * Description:
 * This function creates a directory entry. It decides between making one directory
 * and making a chaing of directories by the buffer_size. As a path string comes from userspace
 * if the path is longer than 8 chars (max lenght of the dir name) it makes a chain of directories
 *
 * Return: STATUS_OK || STATUS_ERROR
 */
static int create(const request_table *req) {
    DEBUG_FS_TASK("[FS_TASK][CREATE]: Creating a new directory for %s\n", req->path);
    uint32_t base_directory = f32_fs.root_cluster;
    dir_traversal_t *map    = dir_get_direction(req->caller_pid);

    if (map != NULL) {
        DEBUG_FS_TASK("[FS_TASK][CREATE]: Using base_directory_cluster: %d\n", map->current_cluster);
        base_directory = map->current_cluster;
    }

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

    return STATUS_OK;
}

/**
 * write - Writes to a file.
 * @param req: pointer to an request_table entry
 *
 * Description:
 * This function writes the contents of req->buf to an opened file.
 * First it looks for the fd entry that holds the file metadata. If found
 * it checks if the requester is allowed to write to the file. after that
 * the contents are written. If succesfull then the directory metadata is updated
 *
 * Return: STATUS_OK || STATUS_ERROR
 */
static int write(const request_table *req) {
    fd_entry_t *entry = fd_entry_table[req->fd];
    if (entry == NULL) {
        ERROR("[FS_TASK][WRITE]: entry not found\n");
        return STATUS_ERROR;
    }

    if (!(entry->flags & O_WRONLY) && !(entry->flags & O_RDWR)) {
        ERROR("[FS_TASK][WRITE]: flag mismatch\n");
        return STATUS_ERROR;
    }

    if (fat32_write_file_at_offset(entry->file_cluster, entry->curr_offset,
                                   (const uint8_t *)req->buf, req->buffer_size) == STATUS_ERROR) {
        ERROR("[FS_TASK][WRITE]: Could not write to opened file\n");
        return STATUS_ERROR;
    }

    DEBUG_FS_TASK("[FS_TASK][WRITE]: Write was successful\n");
    entry->curr_offset = req->buffer_size;
    entry->size        = req->buffer_size;

    if (fat32_update_dirent_size(entry->dir_cluster, entry->file_cluster,
                                 entry->size) == STATUS_ERROR) {
        ERROR("[FS_TASK][WRITE]: Could not update file\n");
        return STATUS_ERROR;
    }

    DEBUG_FS_TASK("[FS_TASK][WRITE]: current offset %d and size %d\n",
                  entry->curr_offset, entry->size);
    return STATUS_OK;
}

/**
 * close - closes the opened file.
 * @param req: pointer to an request_table entry
 *
 * Description:
 * This function closes and frees fd entry of opened file
 *
 * Return: STATUS_OK || STATUS_ERROR
 */
static int close(const request_table *req) {
    fd_entry_t *entry = fd_entry_table[req->fd];
    if (entry == NULL) {
        ERROR("[FS_TASK][CLOSE]: entry not found\n");
        return STATUS_ERROR;
    }

    DEBUG_FS_TASK("[FS_TASK][CLOSE]: Closing fd: %d\n", entry->fd);
    fd_entry_table[req->fd] = NULL;
    kfree(entry);
    return STATUS_OK;
}

/**
 * find - finds a directory of the given path.
 * @param req: pointer to an request_table entry
 *
 * Description:
 * This function tries to find the next file based on the current working directory
 * either going backwards or going forwards
 *
 * Context: While name is missleading it is used by cd command.
 * Return: STATUS_OK || STATUS_ERROR
 */
static int find(const request_table *req) {
    uint8_t direction         = forwards;
    uint32_t current_cluster  = 0;
    uint32_t prev_cluster     = 0;

    uint32_t file_size        = 0;
    uint8_t file_attr         = 0;
    uint32_t starting_cluster = f32_fs.root_cluster;
    const char *path          = (char *)req->path;
    char filename[TASK_NAME_LENGTH];

    if (strcmp(path, "../") == 0 || strcmp(path, "..") == 0) {
        direction = backwards;
    }

    dir_traversal_t *map = dir_get_direction(req->caller_pid);

    if (map != NULL) {
        DEBUG_FS_TASK("[FS_TASK][FIND]: directions found. starting cluster %d\n", map->current_cluster);
        starting_cluster = map->current_cluster;
    }

    if (direction == forwards) {
        if (fat32_find_cluster(starting_cluster, path, &current_cluster, &prev_cluster, &file_size, filename, &file_attr) ==
            STATUS_ERROR) {
            ERROR("[FS_TASK][FIND]: Could not find file.\n");
            return STATUS_ERROR;
        }
    } else if (direction == backwards && map != NULL) {
        DEBUG_FS_TASK("[FS_TASK][FIND]: Going backwards\n");
        current_cluster = map->prev_cluster;

        DEBUG_FS_TASK("[FS_TASK][FIND]: current_cluster: %d\n", current_cluster);

        if (fat32_find_parent_cluster(current_cluster, &prev_cluster) == STATUS_ERROR) {
            ERROR("[FS_TASK][FIND]: Couldnt find parent\n");
            return STATUS_ERROR;
        }
    }

    if (dir_traversal_mapper(req->caller_pid, current_cluster, prev_cluster) == STATUS_ERROR) {
        DEBUG_FS_TASK("[FS_TASK][FIND]: Making a traversal, map failed\n");
        return STATUS_ERROR;
    }

    return STATUS_OK;
}

/**
 * fs_return_vdir_tasks - Returns formated list of tasks.
 * @param req: pointer to an request_table entry
 *
 * Description:
 * This function makes a directory looking lists of tasks currently created.
 *
 * Return: STATUS_OK || STATUS_ERROR
 */
int fs_return_vdir_tasks(request_table *req) {
    DEBUG_FS_TASK("[FS_TASK][R_VIRT_DIR_TASKS]: Returning vdir tasks\n");
    int read_size     = 0;

    char *list_buffer = (char *)kmalloc(req->buffer_size + 1);

    if (list_buffer == NULL) {
        ERROR("[FS_TASK][R_VIRT_DIR_TASKS]: Was unable to allocate buffer. Aborting\n");
        return STATUS_ERROR;
    }

    for (int i = 0; i < MAX_TASKS; i++) {
        if (virt_tasks_dir[i].slot_used == 1) {
            char spid[10];
            itoa(virt_tasks_dir[i].pid, spid);

            int spid_len       = strlen(spid);
            int name_len       = strlen(virt_tasks_dir[i].name);
            int required_space = spid_len + 1 + name_len + 1;

            if (read_size + required_space > (int)req->buffer_size) {
                break;
            }

            memcpy(&list_buffer[read_size], spid, spid_len);
            read_size += spid_len;

            list_buffer[read_size] = '/';
            read_size += 1;

            memcpy(&list_buffer[read_size], virt_tasks_dir[i].name, name_len);
            read_size += name_len;

            list_buffer[read_size] = '\n';
            read_size += 1;
        }
    }

    list_buffer[read_size] = '\0';

    DEBUG_FS_TASK("[FS_TASK][R_VIRT_DIR_TASKS]: Read size %d \n", read_size);
    memcpy(req->buf, list_buffer, read_size);
    req->buffer_size = read_size;
    kfree(list_buffer);
    return STATUS_OK;
}

/**
 * list - Lists directory contents.
 * @param req: pointer to an request_table entry
 *
 * Description:
 * This function lists the contents of the current directory. It checks if there is a
 * directory_traversal entry for the requester to determine the starting cluster. Otherwise,
 * it uses the root cluster. The directory contents are read into a local buffer, then
 * copied to the request_table req->buf buffer, and the read size is updated.
 *
 * Context: Used by the ls command to view contents of the current working directory.
 * Return: STATUS_OK || STATUS_ERROR
 */
static int list(request_table *req) {

    uint32_t base_cluster = f32_fs.root_cluster;
    uint32_t read_size    = 0;
    dir_traversal_t *map  = dir_get_direction(req->caller_pid);

    if (map != NULL) {
        DEBUG_FS_TASK("[FS_TASK][LIST]: directions found. starting cluster %d\n", map->current_cluster);
        base_cluster = map->current_cluster;
    }

    uint8_t *names_buffer = (uint8_t *)kmalloc(req->buffer_size);

    if (names_buffer == NULL) {
        ERROR("[FS_TASK][LIST]: Could not allocate a buffer at this time.\n");
        return STATUS_ERROR;
    }

    fat32_list_dir(base_cluster, names_buffer, &read_size);

    if (read_size > 0) {
        memcpy(req->buf, names_buffer, read_size);
    }

    req->buffer_size = read_size;
    kfree(names_buffer);

    return STATUS_OK;
}

/**
 * free - Frees resources allocated for a task.
 * @param req: pointer to an request_table entry
 *
 * Description:
 * This function cleans up file system structures when a task exits. It iterates through the
 * fd_entry_table and dir_map arrays, freeing every entry that matches the caller_pid of the request
 * and clearing the slots back to NULL.
 *
 * Context: Called during task cleanup to prevent memory leaks.
 * Return: STATUS_OK || STATUS_ERROR
 */
static int free(request_table *req) {
    DEBUG_FS_TASK("[FS_TASK][FREE]: Freeing allocated tables for %d\n", req->caller_pid);

    for (int i = 0; i < MAX_FD_ENTRIES; i++) {
        if (fd_entry_table[i] != NULL && fd_entry_table[i]->owner_pid == req->caller_pid) {
            DEBUG_FS_TASK("[FS_TASK][FREE]: FD entry found, freeing\n");
            kfree(fd_entry_table[i]);
            fd_entry_table[i] = NULL;
        }
    }

    for (int i = 0; i < MAX_TASKS; i++) {
        if (dir_map[i] != NULL && dir_map[i]->owner_pid == req->caller_pid) {
            DEBUG_FS_TASK("[FS_TASK][FREE]: directory map entry found, freeing\n");
            kfree(dir_map[i]);
            dir_map[i] = NULL;
            break;
        }
    }

    DEBUG_FS_TASK("[FS_TASK][FREE]: Freeing succesfull\n");
    return STATUS_OK;
}

/**
 * fs_handle_request - Dispatches incoming file system requests.
 * @param req: pointer to an request_table entry
 *
 * Description:
 * This function processes requests sent to the file system task. It intercepts requests for
 * virtual paths like SYS_INFO/TASKS first, then falls back to a switch-case structure to execute
 * the appropriate file operation handler. It updates the request status and wakes up the caller.
 *
 * Context: The main entry point for the file system server task loop.
 */
void fs_handle_request(request_table *req) {
    task_t *fs_task = task_get_by_pid(fs_task_pid);

    if (fs_task == NULL || req == NULL) {
        return;
    }

    if (req->request_type == LIST && strncmp(req->path, "SYS_INFO/TASKS", sizeof(req->path)) == 0) {
        req->status = (fs_return_vdir_tasks(req) == STATUS_OK) ? COMPLETE : FAILED;
        goto on_success;
    }

    switch (req->request_type) {
    case OPEN:
        req->status = (open(req) == STATUS_OK) ? COMPLETE : FAILED;
        goto on_success;
    case READ:
        req->status = (read(req) == STATUS_OK) ? COMPLETE : FAILED;
        goto on_success;
    case WRITE:
        req->status = (write(req) == STATUS_OK) ? COMPLETE : FAILED;
        goto on_success;
    case CLOSE:
        req->status = (close(req) == STATUS_OK) ? TERMINATED : FAILED;
        return;
    case CREATE:
        req->status = (create(req) == STATUS_OK) ? COMPLETE : FAILED;
        goto on_success;
    case FIND:
        req->status = (find(req) == STATUS_OK) ? COMPLETE : FAILED;
        goto on_success;
    case LIST:
        req->status = (list(req) == STATUS_OK) ? COMPLETE : FAILED;
        goto on_success;
    case FREE:
        req->status       = (free(req) == STATUS_OK) ? COMPLETE : FAILED;
        fs_task->priority = PRIORITY_NORMAL;
        return;
    default:
        ERROR("[FS_TASK][HANDLE_REQUEST]: invalid request type\n");
        req->status = FAILED;
        scheduler_wake_task(req->caller_pid);
        return;
    }

on_success:
    fs_task->priority = PRIORITY_NORMAL;
    scheduler_wake_task(req->caller_pid);
    return;
}

/**
 * fs_maintain_virt_dir - Updates the virtual task directory listing.
 *
 * Description:
 * This function clears the virt_tasks_dir array and rebuilds it by polling the active task list.
 * It skips kernel tasks and dead tasks, copying the PID and name of active user tasks into the
 * virtual directory table so they can be exposed to userspace.
 *
 * Context: Called to keep the virtual task path up to date with the system state.
 */
void fs_maintain_virt_dir() {

    DEBUG_FS_TASK("[FS_TASK][M_VIRT_DIR]: Maintaining virtual directory\n");
    memset(virt_tasks_dir, 0, sizeof(virt_tasks_dir));

    for (int i = 0; i < MAX_TASKS; i++) {
        task_t *temp_task = task_get_by_index(i);

        if (temp_task == NULL || temp_task->task_mode == KERNEL_TASK || temp_task->state == TASK_DEAD) {
            continue;
        }

        virt_tasks_dir[i].pid = temp_task->pid;

        strncpy(virt_tasks_dir[i].name, temp_task->name, sizeof(virt_tasks_dir[i].name) - 1);
        virt_tasks_dir[i].name[sizeof(virt_tasks_dir[i].name) - 1] = '\0';

        virt_tasks_dir[i].slot_used                                = 1;
        DEBUG_FS_TASK("[FS_TASK][M_VIRT_DIR]: Added %d - %s to virtual directory\n", virt_tasks_dir[i].pid, virt_tasks_dir[i].name);
    }
}
