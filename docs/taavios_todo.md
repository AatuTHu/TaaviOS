**Taavi OS TODO**

**Ledger Protocol**
- Reaper dead request cleanup -> scan for TERMINATED, kfree, NULL slot. Woken by Ledger on collect

**Reaper**
- Expand Reaper to handle dead request cleanup -> scan for TERMINATED, kfree, NULL slot
- Reaper woken by Ledger on collect, not polling

**Fs_task**
- if closing a file fails we set the req as failed but dont do anything to it after that -> continue.

**Kernel / Syscall**
- sys_chdir -> stub. Needs cwd, directory attribute check, path resolution
- O_CREAT for files -> not started. Needs path traversal helper first
- Flag constants not in a shared userspace-visible header yet
- Dead variable `entry` in READ case of old collect_request -> gone with fs_task_interact.c

**Filesystem**
- Path traversal helper -> takes full path, returns parent cluster
- curr_offset open behavior not finalized
- sys_delete -> not started
- sys_open, sys_read, sys_write, sys_close, sys_mkdir, sys_mkdirp -> working
- update dir -> not working. if used the sysbin folder that is created with make

**VGA**
- VGA mapped directly in paging -> should go through VMM like all other memory
- VGA driver poorly written -> scroll maybe broken
- Scroll up/down on arrow key press