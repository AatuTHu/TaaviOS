# TaaviOS
A 32 & 64-bit x86 hobby OS exploring combination of micro and monolith kernel architectures called 'microlithic'.

## Architecture

TaaviOS uses a microlithic kernel architecture. Service tasks, referred to as clerks to distinguish them from standard Ring 3 tasks, execute in Ring 0. Clerks reuse the standard task_t structure but do not allocate their own dedicated page directories or user stack. Instead, they map directly into the shared kernel directory located in the upper address space of every regular task.

This design enables seamless, low-overhead system call handling via an asynchronous ledger. When a user task posts a request, the target clerk wakes from sleep and becomes eligible for scheduling. Because the clerk resides within the upper memory region of the active task's page directory, context switching to the clerk requires zero page table swaps and no TLB flushes. Furthermore, because requests are processed asynchronously through the ledger, the CPU remains free to schedule and execute other ready tasks while the clerk fulfills the request in the background.

Clerks leverage three custom protocols during servicing:
* **Ledger Protocol (Asynchronous IPC):** When a user task makes a request, it makes a request via ledger, then the task is marked blocked, and it yields the CPU. Unlike synchronous syscalls, the clerk does not execute inside the caller's immediate context. Instead, the scheduler picks the clerk as an independent task. Because clerks are subject to PIT preemption, a clerk can process requests across multiple ticks with other user tasks until it signals completion and wakes the caller back up.
* **Blankie Protocol:** After all requests are completed, the clerk uses the blankie protocol to reset its context (EIP, ESP, EBP, (or the 64b equivalents)). This resets it to its entry point with a clean stack, after which it sleeps until the next request arrives. The reason for this reset is to prevent stack overflows should the clerk run continuously, while also providing a predictable starting point.
* **Hail Mary Protocol:** If a kernel clerk triggers an exception, the `isr_handler` intercepts the fault before it can panic and shutdown the system. The exception vector routes to `activate_hail_mary`, which invokes the clerk's registered recovery fallback. This purges the exact ledger request that caused the crash, marks it `TERMINATED` to release the blocked user process with an error, and triggers a blankie reset for the clerk.

### Clerks

* **GUI:** Responsible for screen operations, whether drawing to a program window, deleting a window, or hiding it. It also orchestrates all windows on the screen, acting as a window manager.
* **FS:** Responsible for filesystem operations. It reads, writes, opens, and closes files for user programs. It also holds directory traversal data for every task that changes its working directory, alongside creating a virtual directory with system information (memory usage, tasks, CPU time).
* **Reaper:** Handles the cleanup of dead tasks. When a task is killed, all clerks are sent a request to free the memory they reserved for that task. The reaper then tears down the task's page directory and deletes all related resources.
* **Idle:** Runs when there is literally nothing else to do. It simply halts the CPU until the next interrupt.

## Status

Kernel requests basics like fb, mp, memmap, rsdp, modules from limine. Should responses be satisfactory the kernel moves on to initialize GDT, IDT, ISR, TSS, PAGING, PMM, HEAP, DRIVERS, MICROLITHIC ARCH, ACPI, IOAPIC/LAPIC, AP CORES, SCHEDULER, SYSCALLS. After these it frees memory that has been marked to be reclaimable, loads init module. As first task it picks idle and yields immediately. This puts the kernel in to scheduling cycle that at some point picks init task resulting in jump to Ring 3.

After GRUB successfully hands control over, the kernel initializes the GDT, IDT, ISR, TSS, PAGING, PMM, and transitions to protected mode. Following that, it creates the clerks, initializes the file system, programmable interrupt timer, and heap memory manager, and finally creates the init task and jumps to Ring 3 to execute.

## Tradeoffs of Microlithic Kernel Model

In essence, a microlithic kernel combines the high performance of a monolithic kernel with the modular organization of a microkernel. Consequently, it inherits aspects of both designs maximizing speed while requiring specific safeguards for fault tolerance.

### Highlights
* **Simple IPC:** Because clerks execute within the active user task's page directory, requests via the Ledger Protocol require absolutely no physical or virtual memory copying (CR3 switches are completely bypassed). This provides the performance speed of a standard monolithic system call with the modular structural isolation of a microkernel.
* **Predictable Lifecycle:** The Blankie Protocol ensures clerks maintain a clean stack baseline, mitigating long-term state degradation or deep call stack overflows.

### Risks & Mitigations
* **Shared Fault Domain:** Running more service code in Ring 0 inherently increases the risk of system-wide failures or deadlocks compared to pure microkernels.
* **The Hail Mary Safeguard:** To combat Ring 0 panic risks, the Hail Mary Protocol acts as a safety net intercepting Ring 0 exceptions, safely releasing the blocked user process with an error, and resetting the faulting clerk without bringing down the entire kernel.

// A.H - 2026
