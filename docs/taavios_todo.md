### TODO

** SOMETHING TO THINK ABOUT WHILE MAKING FUNCTIONS.
*  Preconditions — what must be true before the function runs. What the caller promises.
*  Postconditions — what will be true after the function runs. What the function promises back.
*  Is this a single use functions? Can it be used again as is or can it be modified to be smart funtion to do essentialy the same thing but with different outcome? Possibly by flags
*  Read, implement, read, learn, implement. No one understands this straight away.

- [30-70%] NO MAGIC NUMBERS
- [x] a way of switching active tasks. For example from one shell to another
- [x] Shell -> ring 0 -> sys_read -> fs_task -> fat32 -> save result -> wake shell -> shell collects -> prints to screen in ring 3
- [x] test functions? (cppcheck for now).
- [x] GUI
- [x] Draw "vuokaavio" to your own notepad
- [x] Gui master window management table.
- [ ] Gui scroll screen up and down
- [ ] Shell arrow drawersal
- [ ] fs_int more complex filesystem management.
- [ ] First real text editor
- [ ] Keyboard routing fixes
- [ ] fs task to create the virtual directory

**Fs_task**
- if closing a file fails we set the req as failed but dont do anything to it after that -> continue.

**Kernel / Syscall**
- O_CREAT for files -> not started.
- Flag constants not in a shared userspace-visible header yet

**Filesystem**
- curr_offset open behavior not finalized
- sys_delete -> not started
- update dir -> not working. if used the sysbin folder that is created with make

**VGA** <- kinda on retirement
- VGA mapped directly in paging -> should go through VMM like all other memory
- VGA driver poorly written -> scroll maybe broken
- Scroll up/down on arrow key press

**Memory**
- Look in to how we can allocate more heap if it is at the end' <- Was able to build this. however
  It can allocate more space for heap. But if the space if deallocated it should be returned? That is the whole point to not have huge amount of heap at the start?

**GUI_TASK**
- Calculate how many lines there can be at a window and do the scrolling by that count instead of offset at a height - padding. 
  height/16 = lines in window. Each x > width or \n adds to y a one. Might aswell divide the window to x slots.