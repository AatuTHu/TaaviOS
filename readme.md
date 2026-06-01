# TaaviOS - 32bit x86 operating system

### Yet another x86 operating system?

Yes.

### Why?

Yes.

### Does it have anything special?

### The Microlithic Kernel

TaaviOS uses a kernel architecture I designed called the **Microlithic Kernel**. a Monolithic foundation organised around microkernel-inspired, domain-owning tasks.

Everything lives in kernel space and shares a single page directory, as in a monolithic kernel. But rather than one large undifferentiated body of code, the kernel is divided into autonomous tasks each one the sole owner of a specific domain. A filesystem task owns all filesystem state. A future network task would own all network state. No kernel task reaches into another's domain. Userspace acts as the conductor, making vertical requests to each task independently. Kernel tasks never talk to each other horizontally.

The design was inspired by asynchronous backend service models. A kernel task is essentially a long-running background service: it sleeps when idle, wakes on demand, does its work, and goes back to sleep. The goal was a kernel that feels like an asynchronous servant of userspace rather than a synchronous gatekeeper.

Since all kernel tasks share the same address space, userspace requests can be dropped directly into task queues during a syscall no expensive context switches, no message-passing overhead. Each task enforces its own memory boundaries in software, actively validating allocations before operating on them. This is not hardware-enforced isolation it is a deliberate tradeoff: less overhead, higher trust requirements per task.

I call it Microlithic monolithic in structure, microkernel-inspired in organisation.

### The Blankie Protocol

After all requests are handled and any background services are complete, each kernel task performs the **Blankie Protocol**.

The task resets its own execution context back to its entry point with a clean stack, marks itself as *sleeping*, and halts. The scheduler ignores sleeping tasks entirely not blocked waiting for something, but intentionally dormant with nothing left to do. When new work arrives and the task is woken, it resumes fresh with no leftover stack state from the previous run cycle.

This eliminates an entire class of hard-to-reproduce bugs: stale stack frames, stale local state, and unbounded stack growth across many request cycles all become impossible by design.

The protocol is registered at task init time via `blankie_register` and activated via `blankie_activate`. Any kernel task gets it for free.

### Tradeoffs

1. Shared kernel address space -> No IPC serialisation or context-switch overhead for kernel requests 
2. Software-level task isolation -> Lower hardware cost; relies on disciplined boundary checking per task 
3. No horizontal task communication -> Eliminates locking complexity; userspace coordinates instead 
4. No privilege separation inside Ring 0 -> A severe task bug can corrupt other kernel memory. accepted risk 

#### 2. Scheduler

The scheduler is a preemptive, priority-aware round-robin design driven by the PIT at 1000Hz.
At each tick, the scheduler considers only tasks that are genuinely ready to run. Dead tasks, blocked tasks, and sleeping tasks are skipped entirely so they do not consume scheduling time. The ready queue is the only thing that matters at switch time.

Priority levels exist but do not override fairness within a level. A higher-priority task gets preference when it is ready, but tasks at the same priority rotate equally. This keeps the design predictable: nothing starves, and the behaviour under load is easy to reason about.

Task cleanup is deliberately separated from the scheduler. A dead task is not destroyed at the moment it exits. It is marked dead and left for a dedicated reaper task to collect. This keeps the context switch path short and free of allocation or deallocation work. A context switch does one thing: pick the next ready task and switch to it.

The distinction between blocked and sleeping is intentional and meaningful. A blocked task is waiting for something external like a keyboard input, a filesystem response, an event. A sleeping task, by contrast, is a kernel task that has finished all its current work and gone dormant via the Blankie Protocol. Both are invisible to the scheduler, but for different reasons. Blocked tasks are woken by external events. Sleeping tasks are woken by new work arriving in their queue.

Kernel clerk tasks interact with the scheduler through a separate check that runs alongside the normal scheduling pass. If a clerk has pending work, it is activated before the next userspace task runs. This ensures filesystem requests, and eventually network or GUI work, are serviced promptly without giving kernel tasks unconditional scheduling priority over userspace.


#### 3. The "Learn as You Go" Engineer

My background is in web and mobile development. JavaScript, React, React-native. I have an engineering degree in Information and Communications Technology, but nothing in my education (only had a one course in C :D) or day job pointed toward operating systems, low-level C, or NASM assembly. I started this project knowing none of it.

The learning process has been books, the OSDev wiki, other people's source code, YouTube, and using AI as a sounding board for concepts that wouldn't click from text alone. Every part of this kernel was written after educating myself on the topic, writing it, failing and then writing it again. So not just copying a working black box implementation.

The parts I have enjoyed most are the ones that required the most original thinking: the scheduler and the Microlithic Kernel model. Getting preemptive multitasking or the asynchronous open request working for the first time was a particular milestone.

### Current Status

- Arch: 32-bit Protected Mode, higher-half kernel, paging enabled
- Memory management: PMM with bitmap allocation, VMM, kmalloc/kfree with block splitting and merging
- Preemptive round-robin scheduler at 1000Hz with task-isolated address spaces
- Microlithic kernel with fs_task, full request queue, fd table, and Blankie Protocol
- FAT32 read/write driver with path resolver and subdirectory traversal
- `sys_open`, `sys_read`, `sys_write` syscall interface via `int 0x80`
- ELF loader, init process, interactive shell
- Small C userspace library

*Author: A.H ~ 2026*