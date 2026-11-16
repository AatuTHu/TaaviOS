# TaaviOS

A 32 & 64-bit x86 hobby OS that explores mixing microkernel and monolithic ideas. I call this approach "microlithic". All because I didn't want to put the FD number in the task structure.

## Architecture

TaaviOS runs its services as tasks called clerks. Clerks use the standard `task_t` structure but execute in Ring 0, and they have no page directory or user stack of their own. Instead they live in the kernel half of the address space, which is mapped into every regular task's page directory.

User tasks talk to clerks through an asynchronous request queue called the ledger. When a task posts a request, the clerk wakes up and becomes eligible for scheduling. Because the clerk is already mapped in the active task's address space, switching to it needs no CR3 switch and no TLB flush. Requests are processed asynchronously, so the CPU can keep running other ready tasks while the clerk works. This is more of a win on the 32-bit arch, as it is a single core system and a TLB flush hurts the most there.

Clerks use three protocols during their work loop:

* **Ledger Protocol (async IPC):** This is the only protocol that user tasks also use. The other two are clerk-internal. A task posts a request to a clerk and the clerk picks the request up and performs it to the best of its abilities (or to the best of my programming skills?). After the request has failed or completed, the clerk signals the scheduler to switch the task to the runnable state again. The task then collects the results and the request is set to the terminated state. After a while the reaper deletes the request and frees the memory it was using.

Some examples of requests are window buffer creation, resizing, killing another task, moving a window, opening a file, creating a directory or requesting the doc to do doc stuff.

* **Blankie Protocol:** Once a clerk has finished its requests, it resets its context (EIP, ESP, EBP, or the 64-bit equivalents) to the entry point with a clean stack, then sleeps until the next time servicing is needed.

* **Hail Mary Protocol:** If a clerk triggers an exception, `isr_handler` catches it before it becomes a system wide panic and routes it to `activate_hail_mary`. That calls the clerk's registered recovery function, which marks the offending ledger request `TERMINATED` (releasing the blocked task with an error) and forces an early blankie reset.

### Clerks

When not servicing requests the clerks can do other work, like maintaining the virtual directory, refreshing buffers to the framebuffer and printing system info to the shell.

* **GUI:** Responsible for framebuffer operations. Creates, resizes, moves, deletes all the while keeping track of pixel buffers and dimensions for each task.
Also responsible for blitting dirty buffer to video memory.

* **FS:** Responsible for filesystem operations. It reads, writes, opens, and closes files for user programs. It also holds directory traversal data for every task that changes its working directory, alongside creating a virtual directory of tasks.

* **Reaper:** Handles the cleanup of dead tasks. When a task is killed, all clerks are sent a request to free the memory they reserved for that task. The reaper then tears down the task's page directory and deletes all related resources.

* **Doc:** Tests system functions. For example, it tests the FS clerk's functionality (once went a month without noticing some functions were not working anymore). Occasionally prints ledger statistics and core statistics to screen.

* **Idle:** Runs when there is literally nothing else to do. It simply halts the CPU until the next interrupt.

## Status

Depending on which arch you build, the kernel boots via GRUB (x86) or Limine (x86_64). On x86, system information arrives in multiboot 1 format, collected in real mode between GRUB and the kernel, and the kernel fakes a paging-enabled state before switching to its own stack. On x86_64, Limine already puts the CPU in long mode, so the kernel requests system information from Limine and only has to switch to its own stack. Both then make a far jump to their kernel entry.

After the jump both initialize the physical memory manager, paging, virtual memory manager, heap, GDT, IDT, framebuffer, syscalls, scheduler, FAT32 and the clerks. The 32-bit version uses the PIT and runs on a single core. The 64-bit version uses SMP, so it also sets up the IOAPIC, LAPIC and the other cores.

Both arches then create `init` from the one kernel module provided. On 32-bit the kernel jumps straight into it. On 64-bit the boot core does the same, while the other cores start in idle and simply yield, which gets them scheduling tasks.

## Tradeoffs

### My favourites
* The scheduler has three priorities (high, normal, low), but at most three high-priority picks in a row.
* Requests to a clerk need no CR3 switch, which should make them cheaper than switching to a user-space server.
* Clerks can also be programmed to run as plain kernel threads.
* The Blankie Protocol keeps clerk stacks from growing unbounded.
* The Hail Mary Protocol has saved the kernel a few times and kept user-space responsive.
* Symmetric multiwinning (SMP).

### Risks
* **Shared fault domain:** More service code in Ring 0 raises the chance of system-wide failures or deadlocks compared to a pure microkernel.
* **Hail Mary:** It catches Ring 0 exceptions in clerks, releases the blocked process with an error and resets the clerk. It can't help if a clerk corrupts shared kernel state or deadlocks while holding a lock.

// A.H - 2026
