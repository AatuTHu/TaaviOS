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
- [x] User space needs malloc/alloc
- [ ] GUI scroll screen up and down
- [50%] left and right working Shell arrow traversal
- [ ] Blinking cursor
- [ ] fs_int more complex file system management.
- [ ] First real text editor
- [x] Virtual tasks return adds random chars at the end of the list
- [ ] Batch gfx operations. (Many requests on one sys call)



