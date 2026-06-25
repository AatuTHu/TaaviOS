**Taavi OS TODO**

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
  
**SCHEDULER**
- Wakes clerks continuesly even tho they are awake
