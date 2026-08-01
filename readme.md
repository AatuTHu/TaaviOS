# TaaviOS
A 32-bit x86 hobby OS exploring async kernel architecture and fault tolerance.

## Architecture
TaaviOS uses a **microlithic kernel** model designed and implemented by me. It has service kernel tasks ("clerks") that run in Ring 0. Clerks use the same existing task_t structure but they do not get their own page directory. This allows incoming system calls to make a seamless request to clerks via ledger. After a request has been made, the clerk is awoken from sleep and is then able to be picked as a running task by the scheduler. When a clerk is picked, the scheduler does not force a cr3 switch because all clerks reside on the active user task's page directory. Clerks leverage three custom protocols during servicing:

**Ledger Protocol** Requests go through a shared table (caller submits -> clerk is awoken -> clerk fetches -> clerk signals completion -> caller collects). This is the *only* way to interact with a clerk. A clerk is able to contact another clerk via ledger protocol by making the same moves as user tasks.

**Blankie Protocol** After all requests are completed, the clerk uses the blankie protocol to reset its context (EIP, ESP, EBP). This resets it to its entry point with a clean stack. Then it sleeps until the next request arrives. Reason for this reset is to prevent stack overflow should the clerk run for days and also have a predictable starting point for the clerks.

**Hail Mary Protocol** If a kernel clerk triggers an exception, the isr_handler intercepts the fault before it can panic and shutdown the system. The exception vector routes to activate_hail_mary, which invokes the clerk's registered recovery fallback (fs_recovery). This purges the exact ledger request that caused the crash, marks it TERMINATED to release the blocked user process with an error, and triggers a Blankie reset. The clerk restarts with a clean stack and the kernel survives.

### Clerks

**Gui** Gui is responsible for screen operations whether it is to draw a screen to a program window, delete the window, or hide it. It also orchestrates all the windows on the screen acting as a window manager.

**FS** FS is responsible for the filesystem operations. It reads, writes, opens, and closes files for the user programs. It also holds the directory traversal data for every task that changes its working directory. Other tasks include creating a virtual directory with system information (memory usage, tasks, CPU time).

**Reaper** Reaper handles cleanup of dead tasks and finished ledger requests. When a task is killed or a request fails, the reaper frees the associated memory and resources so nothing leaks.

**Idle** Idle runs when there is literally nothing else to do. It simply halts the CPU until the next interrupt.

## Status

* [x] Protected mode, paging, GDT/IDT/TSS
* [x] PMM, VMM, kmalloc/kfree with dynamic heap expansion
* [x] Preemptive priority-aware round-robin scheduler (1000Hz)
* [x] FAT32 read/write, mkdir, mkdirp, path resolver
* [x] ELF loader, userspace, interactive shell
* [x] System calls: sys_open, sys_read, sys_write, sys_close, sys_exec, sys_mkdir, sys_chdir, sys_getdents
* [x] Clerks: Fs_task, Reaper_task, Gui_task, Idle_task
* [x] Blankie, Hail Mary, and Ledger protocols
* [x] GUI with window management, text rendering, and sprite drawing
* [x] Keyboard driver with foreground/background routing
* [x] ATA PIO disk driver with MBR partition detection
* [x] Framebuffer graphics via multiboot VBE
* [x] Virtual SYS_INFO/TASKS directory for live process listing
* [ ] Userspace malloc
* [ ] Proper locking / synchronization
* [ ] Arrow key history, blinking cursor
* [ ] File creation (O_CREAT), file deletion

## Tradeoffs of Microlithic Kernel Model
In its essence, the microlithic kernel is a monolith with the organization of a microkernel.

### Advantages
**Zero-Overhead IPC:** Because clerks execute within the active user task's page directory, requests via the Ledger Protocol require absolutely no physical or virtual memory copying (cr3 switches are completely bypassed). This provides the performance speed of a standard monolithic system call with the modular structural isolation of a microkernel.

**Predictable Lifecycle:** The Blankie Protocol ensures clerks maintain a clean stack baseline, mitigating long-term state degradation or deep call stack overflows.

### Risks & Mitigations
**Shared Fault Domain:** Running more code at the Ring 0 level inherently increases the risk of a complete system shutdown or complex deadlocks.
**The Hail Mary Safeguard:** To combat the risk of system-wide panics, the Hail Mary Protocol acts as the last line of defence as it intercepts Ring 0 exceptions, safely releases the blocked user process with an error, and resets the clerk without bringing down the entire kernel.

// A.H — 2026
