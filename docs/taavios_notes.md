### TODO

** SOMETHING TO THINK ABOUT WHILE MAKING FUNCTIONS.
*  Preconditions — what must be true before the function runs. What the caller promises.
*  Postconditions — what will be true after the function runs. What the function promises back.
*  Is this a single use functions? Can it be used again as is or can it be modified to be smart funtion to do essentialy the same thing but with different outcome? Possibly by flags
*  Read, implement, read, learn, implement. No one understands this straight away.

- [30-70%] NO MAGIC NUMBERS
- [ ] vga assigns itselft to a memory addrres without using vmm. paging inits vga tho. Not good.
- [ ] a way of switching active tasks. For example from one shell to another
- [x] Shell -> ring 0 -> sys_read -> fs_task -> fat32 -> save result -> wake shell -> shell collects -> prints to screen in ring 3
- [x] test functions? (cppcheck for now).
- [ ] GUI
- [x] Draw "vuokaavio" to your own notepad
- [ ] fs_task loop can fall to an infinite loop if request is complite but never collected. I need to think of an solution.