# TaaviOS
A 32/64-bit x86 hobby OS exploring asynchronous kernel architecture and fault tolerance.

## Architecture

> **[NOTE]** The features described here work only on the 32-bit architecture, but will be ported to 64-bit later.

TaaviOS uses a **microlithic kernel** model designed and implemented by me. It has service kernel tasks ("clerks") that run in Ring 0. Clerks use the existing `task_t` structure (the same one that is used by userspace tasks), but they do not get their own page directory. This allows incoming system calls to make seamless requests to clerks via a ledger. After a request has been made, the clerk is awoken from sleep and becomes eligible for scheduling. When a clerk is picked, the scheduler does not force a CR3 switch because all clerks reside on the active user task's page directory. Clerks leverage three custom protocols during servicing:

* **Ledger Protocol (Asynchronous IPC):** When a user task makes a request, it makes a request via ledger, then the task is marked blocked, and it yields the CPU. Unlike synchronous syscalls, the clerk does not execute inside the caller's immediate context. Instead, the scheduler picks the clerk as an independent task. Because clerks are subject to PIT preemption, a clerk can process requests across multiple ticks with other user tasks until it signals completion and wakes the caller back up.
* **Blankie Protocol:** After all requests are completed, the clerk uses the blankie protocol to reset its context (EIP, ESP, EBP). This resets it to its entry point with a clean stack, after which it sleeps until the next request arrives. The reason for this reset is to prevent stack overflows should the clerk run continuously, while also providing a predictable starting point.
* **Hail Mary Protocol:** If a kernel clerk triggers an exception, the `isr_handler` intercepts the fault before it can panic and shutdown the system. The exception vector routes to `activate_hail_mary`, which invokes the clerk's registered recovery fallback. This purges the exact ledger request that caused the crash, marks it `TERMINATED` to release the blocked user process with an error, and triggers a blankie reset for the clerk.

### Clerks

* **GUI:** Responsible for screen operations, whether drawing to a program window, deleting a window, or hiding it. It also orchestrates all windows on the screen, acting as a window manager.
* **FS:** Responsible for filesystem operations. It reads, writes, opens, and closes files for user programs. It also holds directory traversal data for every task that changes its working directory, alongside creating a virtual directory with system information (memory usage, tasks, CPU time).
* **Reaper:** Handles the cleanup of dead tasks. When a task is killed, all clerks are sent a request to free the memory they reserved for that task. The reaper then tears down the task's page directory and deletes all related resources.
* **Idle:** Runs when there is literally nothing else to do. It simply halts the CPU until the next interrupt.

## Status

64-bit architecture port in progress.

After GRUB successfully hands control over, the kernel initializes the GDT, IDT, ISR, TSS, PAGING, PMM, and transitions to protected mode. Following that, it creates the clerks, initializes the file system, programmable interrupt timer, and heap memory manager, and finally creates the init task and jumps to Ring 3 to execute.

## Tradeoffs of Microlithic Kernel Model

In essence, a microlithic kernel combines the high performance of a monolithic kernel with the modular organization of a microkernel. Consequently, it inherits aspects of both designs—maximizing speed while requiring specific safeguards for fault tolerance.

### Advantages
* **Zero-Overhead IPC:** Because clerks execute within the active user task's page directory, requests via the Ledger Protocol require absolutely no physical or virtual memory copying (CR3 switches are completely bypassed). This provides the performance speed of a standard monolithic system call with the modular structural isolation of a microkernel.
* **Predictable Lifecycle:** The Blankie Protocol ensures clerks maintain a clean stack baseline, mitigating long-term state degradation or deep call stack overflows.

### Risks & Mitigations
* **Shared Fault Domain:** Running more service code in Ring 0 inherently increases the risk of system-wide failures or deadlocks compared to pure microkernels.
* **The Hail Mary Safeguard:** To combat Ring 0 panic risks, the Hail Mary Protocol acts as a safety net—intercepting Ring 0 exceptions, safely releasing the blocked user process with an error, and resetting the faulting clerk without bringing down the entire kernel.

// A.H — 2026
