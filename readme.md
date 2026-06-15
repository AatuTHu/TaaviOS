# TaaviOS
A 32-bit x86 hobby OS exploring async kernel architecture and fault tolerance.

## Architecture
TaaviOS uses a **microlithic kernel** model designed and implemented by me. It has service kernel tasks ("clerks") that run in Ring 0. Clerks use the same existing task_t structure but they do not get their own page directory. This allows incoming system calls to make a seamless request to clerks via ledger. After a request has been made, the clerk is awoken from sleep and is then able to be picked as a running task by the scheduler. When a clerk is picked, the scheduler does not force a cr3 switch because all clerks reside on the active user task's page directory. Clerks leverage three custom protocols during servicing:

**Ledger Protocol** Requests go through a shared table (caller submits -> clerk is awoken -> clerk fetches -> clerk signals completion -> caller collects). This is the *only* way to interact with a clerk. A clerk is able to contact another clerk via ledger protocol by making the same moves as user tasks.

**Blankie Protocol** After all requests are completed, the clerk uses the blankie protocol to reset its context (EIP, ESP, EBP). This resets it to its entry point with a clean stack. Then it sleeps until the next request arrives. Reason for this reset is to prevent stack overflow should the clerk run for days and also have a predictable starting point for the clerks.

**Hail Mary Protocol** If a kernel clerk triggers an exception, the isr_handler intercepts the fault before it can panic and shutdown the system. The exception vector routes to activate_hail_mary, which invokes the clerk's registered recovery fallback (fs_recovery). This purges the exact ledger request that caused the crash, marks it TERMINATED to release the blocked user process with an error, and triggers a Blankie reset. The clerk restarts with a clean stack and the kernel survives.

## Status
- [x] Protected mode, paging, GDT/IDT/TSS
- [x] PMM, VMM, kmalloc/kfree
- [x] Preemptive priority-aware round-robin scheduler (1000Hz)
- [x] FAT32 read/write, path resolver
- [x] ELF loader, userspace, interactive shell
- [x] sys_open, sys_read, sys_write
- [x] Fs_task, Reaper_task
- [x] Blankie, Hail Mary, ledger protocols
- [ ] GUI, vga

## Tradeoffs
Same as any monolithic kernel: clerks share the address space, so a rogue clerk can corrupt another clerk's memory. Hail Mary contains faults after the fact but doesn't prevent them.

// A.H — 2026