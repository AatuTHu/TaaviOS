### TODO

- [30-70%] NO MAGIC NUMBERS
- [x] a way of switching active tasks. For example from one shell to another
- [x] Shell -> ring 0 -> sys_read -> fs_task -> fat32 -> save result -> wake shell -> shell collects -> prints to screen in ring 3
- [x] test functions? (cppcheck for now).
- [x] GUI
- [x] Draw "vuokaavio" to your own notepad
- [x] Gui master window management table.
- [x] fs task to create the virtual directory (sys_info/tasks, idle-cycle-refreshed static cache)
- [x] sys_caw and sys_conwi merged into single syscall (sys_wi)
- [ ] sys_wi operation to draw image/sprite from ring 3 (blit userspace buffer to window)
- [ ] Userspace needs malloc/alloc
- [ ] Gui scroll screen up and down
- [ ] Shell arrow traversal
- [ ] Blinking cursor
- [ ] fs_int more complex filesystem management.
- [ ] First real text editor
- [ ] Keyboard routing fixes

**Concurrency / Locks**
- Add a locking mechanism in which interruptsa are disabled but only can be active at one place at a time. To prevent deadlocks

**Fs_task**
- if closing a file fails we set the req as failed but dont do anything to it after that -> continue.

**Kernel / Syscall**
- O_CREAT for files -> not started.
- getticks/getuptime syscall not started (needed for cursor blink)

**Filesystem**
- curr_offset open behavior not finalized
- sys_delete -> not started
- update dir -> not working. if used the sysbin folder that is created with make

**VGA / FB**
- VGA/FB mapped directly in paging -> should go through VMM like all other memory <- deferred mapping function added to vmm. fb goes throught this. VGA  still dosnt, but it is not used anyways atm.
- VGA driver poorly written -> scroll maybe broken
- Scroll up/down on arrow key press

**Memory**
- Look into how we can allocate more heap if it is at the end <- Was able to build this. However
  it can allocate more space for heap. But if the space is deallocated it should be returned? That is the whole point to not have huge amount of heap at the start? <- idea cooking
