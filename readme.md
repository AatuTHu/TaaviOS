# TaaviOS - 32bit x86 operating system

### Yet another x86 operating system?

Yes.

### Why?

Yes.

### Does it have anything special?

#### 1. The Microlithic Kernel

TaaviOS uses a kernel architecture I designed called the **Microlithic Kernel**. An asynchronous kernel model that is based on a monolithic kernel with microlithic organization.

The kernel is structured around autonomous kernel tasks or as I like to call them "clerks". Each clerk has its own field of operations. A filesystem clerk does all filesystem related work, a future network task would do all network related. The clerks do not communicate to each other directly so to minimize the risk of deadlock. Essentially there should never be horizontal communication between the clerks as why would a filesystem clerk talk with the guy who does landscaping (gui clerk).

Each clerk is like a long-running background service. A task in userspace can make a request that goes directly into clerks mailboxes via syscalls. So the clerk sleep when idle, wake on request, handle the request and wake the caller. After that the clerks "warms-down" or do other service work to its own field. For example fs_task keep up the virtual file (yet to be implemented) all the while the userspace task has continued its stack.

Because the clerk is located on the kernel directory part of the callers page directory it is able to receive the message and communicate results back to it without switching directories.

#### 2. The Blankie Protocol

After all requests and background services have been completed, each kernel task performs the **Blankie Protocol**. Think of it as the end of the road for the clerk.

The task resets its own execution context back to its original entry point with a clean stack, marks itself as *sleeping*, and halts. Then if new requests arrive the task is woken, it resumes fresh with no leftover stack state from the previous run cycle.

The idea came from continued errors and the want to have an predictable path for the tasks to follow. The protocol is registered at task init time via `blankie_register` and activated via `blankie_activate`. Any kernel task gets it for free.

#### 3. The Ledger Protocol

Each kernel task that needs heap memory asks for it by using the **Ledger Protocol**. If the Blankie Protocol was the end of the road, this is the sidelines.

The idea is borrowed from the VMM as it abstracts fragmented physical pages into a continuous virtual address space for a process, the Ledger on the other hand abstracts fragmented heap allocations into a continuous protocol address space for a clerk. Each clerk gets its own floor starting from `0xD0000000`. Allocations within that floor are tracked against the clerk's pid. The real heap address lives only inside the ledger. To use an allocation a clerk presents its pid and protocol address then the ledger validates ownership and returns the real address. Nothing outside the ledger holds a real address directly. 

#### 4. Portability

The Blankie and Ledger Protocols and the Microlithic model itself are architecture independent. The current implementation targets x86 but nothing in the design requires it. The concepts port to any architecture with virtual memory and privilege separation.

#### 5. Tradeoffs

All kernel tasks run in ring 0 and share the same address space. A misbehaving clerk can corrupt another clerk's memory. This is the standard monolithic tradeoff. Even Linux has the same risks produced in a different manner. The Ledger Protocol exists to enforce domain boundaries in software and make violations visible. The constraint is disciplined implementation.

#### 6. Scheduler

The scheduler is a preemptive, priority-aware round-robin design driven by the PIT at 1000Hz.
At each tick, the scheduler considers only tasks that are genuinely ready to run. Tasks with any other state are skipped entirely so to not to consume processor time. The ready queue is the only thing that matters at switch time.

Priority levels exist but do not override fairness within a level. A higher-priority task gets preference when it is ready, but tasks at the same priority rotate equally.

Task cleanup is deliberately separated from the scheduler. A dead task is not destroyed at the moment it exits. It is marked dead and left for a dedicated kernel task called the Reaper. Idea is that instead of using processor time to delete tasks the processor can perform another user tasks. 

So we have atleast two kernel tasks that are active simultaneously as is user tasks. Where is the asynchronous of it all? Well while the processor advances the userspace stack this tick, the next tick it is killing another, then userspace, then it is closing said userspaces open folder as requested all the while the user task has already continued thru it's stack.


### Current Status

- Arch: 32-bit Protected Mode, higher-half kernel, paging enabled
- Memory management: PMM with bitmap allocation, VMM, kmalloc/kfree with block splitting and merging
- Preemptive round-robin scheduler at 1000Hz with task-isolated address spaces
- Microlithic kernel with fs_task, full request queue, fd table, Blankie Protocol, and Ledger Protocol
- FAT32 read/write driver with path resolver and subdirectory traversal
- `sys_open`, `sys_read`, `sys_write` syscall interface via `int 0x80`
- ELF loader, init process, interactive shell
- Small C userspace library

### Designer behind this

I have a engineering degree in tech and wanted to try my hand at developing an OS. I have had only one course on C and none of assembly. I have never done a days work in a real tech company. My skills are (were) in programming with javascript using libraries like React or React native.

The learning process has been books, **the OSDev wiki**, r/osdev on reddit, other people's source code, YouTube, and using AI as a sounding board for concepts that wouldn't click from text alone. I try to not to lean too much on tutorials, others work or the generic way of doing things as I'm at my best doing the things in my own way.

Every part of this kernel was written after educating myself on the topic, writing it, failing, and then writing it again then auditing and thinking before ultimately failing again.

The parts I have enjoyed most are the ones that required the most original thinking: the scheduler, the Microlithic Kernel model, the Blankie Protocol, and the Ledger Protocol. Getting preemptive multitasking or the asynchronous open request working for the first time was a particular milestone.

*Author: A.H ~ 2026*