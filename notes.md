### NOTES

** SOMETHING TO THINK ABOUT WHILE MAKING FUNCTIONS.
*  Preconditions — what must be true before the function runs. What the caller promises.
*  Postconditions — what will be true after the function runs. What the function promises back.
*  Is this a single use functions? Can it be used again as is or can it be modified to be smart funtion to do essentialy the same thing but with different outcome? Possibly by flags

* what next?
* Expose filesystem to userspace. Maybe a special task that shell uses? 
* Filesystem as a task is surely a high-priority task ONLY if it is called other wise it is low. 
* And is a kernel task? Hmm Easier to have it as a api but. since it is a slow thing maybe it has to be? 
* A special task that is marked blocked and low. 
* if called it is running and high?
*  Filesystems job is to keep sysfile updated? Sysfile has info on the system? Otherwise its job is to find and show directories and files?

- progress on the above. I have created filesystem task which is opened and can receive requests from userspace. What I need to do now is to make better sys_write before I continue to make fs_task more. Userspace also. There is a elephant in the room tho. Shell is blocked, but it can be unblocked by typing even is fs_task is still doing it's thing


1. NO MAGIC NUMBERS semi done
2. vga assigns itselft to a memory addrres without using vmm. paging inits vga tho. Not good.
3. a way of switching active tasks. For example from one shell to another
4. filesystem -> sysfile -> echo to shell system info
5. test functions?
6. GUI
7. Draw "vuokaavio" to your own notepad
8. Read, implement, read, learn, implement. No one understands this straight away.