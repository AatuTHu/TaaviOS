**Taavi OS TODO**

**Ledger Protocol**
- Ledger put back to retirement as it was causing errors I could not understand

**Reaper**
- Expand Reaper to handle dead request cleanup -> scan for TERMINATED, kfree, NULL slot
- Reaper woken by Ledger on collect, not polling

**Fs_task**
- if closing a file fails we set the req as failed but dont do anything to it after that -> continue.
- cd ../ not working. Cant find parent. Whole cd is kinda sketchy.

**Kernel / Syscall**
- sys_chdir -> stub. Needs cwd, directory attribute check, path resolution
- O_CREAT for files -> not started.
- Flag constants not in a shared userspace-visible header yet

**Filesystem**
- curr_offset open behavior not finalized
- sys_delete -> not started
- sys_open, sys_read, sys_write, sys_close, sys_mkdir, sys_mkdirp -> working
- update dir -> not working. if used the sysbin folder that is created with make
- find_parent_cluster working maybe

**VGA**
- VGA mapped directly in paging -> should go through VMM like all other memory
- VGA driver poorly written -> scroll maybe broken
- Scroll up/down on arrow key press