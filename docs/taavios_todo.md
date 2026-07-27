### TODO

- [30-70%] NO MAGIC NUMBERS
- [x] a way of switching active tasks. For example from one shell to another
- [x] Shell -> ring 0 -> sys_read -> fs_task -> fat32 -> save result -> wake shell -> shell collects -> prints to screen in ring 3
- [x] test functions? (cppcheck for now).
- [x] GUI
- [x] Draw "vuokaavio" to your own notepad
- [x] GUI master window management table.
- [x] fs task to create the virtual directory (sys_info/tasks, idle-cycle-refreshed static cache)
- [x] sys_caw and sys_conwi merged into single system call (sys_wi)
- [x] sys_wi operation to draw image/sprite from ring 3 (blit user space buffer to window)
- [ ] User space needs malloc/alloc
- [ ] GUI scroll screen up and down
- [ ] Shell arrow traversal
- [ ] Blinking cursor
- [ ] fs_int more complex file system management.
- [ ] First real text editor
- [ ] Keyboard routing fixes

**Concurrency / Locks**
- Add a locking mechanism in which interrupts are disabled but only can be active at one place at a time. To prevent deadlocks

**Fs_task**
- if closing a file fails we set the req as failed but don't do anything to it after that -> continue.

**Kernel / System calls**
- O_CREAT for files -> not started.
- get ticks/get uptime system call not started (needed for cursor blink)

**File system**
- curr_offset open behavior not finalized
- sys_delete -> not started
- update dir -> not working. If used the sysbin folder that is created with make

**VGA / FB**
- VGA/FB mapped directly in paging -> should go through VMM like all other memory <- deferred mapping function added to vmm. fb goes through this. VGA  still doesn't, but it is not used anyways at the moment.
- VGA driver poorly written -> scroll maybe broken
- Scroll up/down on arrow key press

**Memory**
- Look into how we can allocate more heap if it is at the end <- Was able to build this. However
  it can allocate more space for heap. But if the space is deallocated it should be returned?
  That is the whole point to not have huge amount of heap at the start? <- idea cooking
- On Task dead the VMM_FREE_USER_SPACE can make an early return. In that case parts of it is destroyed but not all. Ill cook something for this
