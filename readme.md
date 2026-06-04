# TaaviOS
A 32-bit x86 operating system exploring non-traditional asynchronous kernel architecture and fault tolerance.

## Architecture

TaaviOS uses a **Microlithic Kernel** that is modeled after the monolithic foundation but is organized like a microkernel. It uses autonomous kernel clerks who own a single domain (filesystem, networking, etc.) and communicate with userspace exclusively through a request mailbox. Direct horizontal communication between clerks is prohibited to prevent deadlocks.

Clerks share the same task structure as userspace tasks but run with kernel CS/SS segments and the shared kernel page directory. They require no page directory of their own, keeping context switches lightweight. 

When a userspace task needs a service, it submits a request to the clerk's mailbox via syscall, then blocks and yields the CPU. The clerk wakes, processes the request independently, and signals the caller when complete. The caller resumes and collects the result. This decoupling means the CPU is never held waiting and other tasks can run freely while the clerk works.

Two protocols govern clerk behaviour:

**Blankie Protocol** On completing its work, a clerk resets its execution context to its entry point with a clean stack and marks itself sleeping. It resumes only when a new request arrives, guaranteeing no leftover state between cycles.

**Hail Mary Protocol** When a clerk causes a fatal exception, the ISR checks a registered recovery table indexed by PID. If a callback exists, it is invoked to clean up in-flight state and trigger a blankie reset. The clerk recovers; the system keeps running.

## Status

- Protected mode, paging, GDT/IDT/TSS  ✓ 
- PMM, VMM, kmalloc/kfree  ✓ 
- Preemptive round-robin scheduler (1000Hz)  ✓ 
- FAT32 read/write, path resolver  ✓ 
- ELF loader, userspace, interactive shell  ✓ 
- sys_open, sys_read, sys_write  ✓ 
- Blankie, Hail Mary protocols  ✓ 

## Tradeoffs

Clerks run in Ring 0 and share the kernel address space, a rogue clerk can theoretically corrupt another's memory. This is the standard monolithic tradeoff. The Hail Mary Protocol exists specifically to contain these faults without halting the system.

The Microlithic design and Blankie Protocol are architecture-independent, portable to any platform with virtual memory and privilege separation.

// A.H — 2026
