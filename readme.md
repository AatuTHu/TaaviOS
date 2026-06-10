# TaaviOS

A 32-bit x86 hobby OS exploring async kernel architecture and fault tolerance.

## Architecture

TaaviOS uses a **microlithic kernel**: service tasks ("clerks") that run in Ring 0 and share the kernel page directory (like a monolith), but communicate exclusively through an async request mailbox rather than direct calls (like a microkernel). Horizontal clerk-to-clerk calls are prohibited to prevent deadlocks.

**Clerks as scheduler tasks.** A clerk is a `task_t` same struct, same scheduler, same context-switch path as a user process. There's no separate dispatch mechanism. The fs_task is PID 1, reaper_task is PID 2, visible and scheduled alongside user tasks. No CR3 switch needed when scheduling between clerks since they share the kernel page directory.

**Ledger mailbox.** Requests go through a shared table (caller submits -> clerk is awoken -> clerk fetches -> clerk signals completion -> caller collects). This is the *only* way to interact with a clerk.

**Blankie Protocol.** After all requests are complited, the clerk uses blankie protocol to reset its context (EIP, ESP, EBP). This resets its entry point with a clean stack. Then it sleeps until the next request arrives.

**Hail Mary Protocol.** If a clerk faults, the ISR looks up a recovery callback by PID, invokes it to clean up in-flight ledger state, then triggers a Blankie reset. The clerk restarts fresh; the kernel continues.

## Status

- [X] Protected mode, paging, GDT/IDT/TSS  
- [X] PMM, VMM, kmalloc/kfree  
- [X] Preemptive priority-aware round-robin scheduler (1000Hz)  
- [X] FAT32 read/write, path resolver  
- [X] ELF loader, userspace, interactive shell  
- [X] sys_open, sys_read, sys_write  
- [X] Fs_task, Reaper_task  
- [X] Blankie, Hail Mary, ledger protocols  
- [ ] GUI, vga
- [ ] 

## Tradeoffs

Same as any monolithic kernel: clerks share the address space, so a rogue clerk can corrupt another clerk's memory. Hail Mary contains faults after the fact but doesn't prevent them.

// A.H — 2026