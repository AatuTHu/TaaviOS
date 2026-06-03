# TaaviOS - 32bit x86 operating system

### Yet another x86 operating system?

Yes.

### Why?

Yes.

### Does it have anything special?

#### 1. The Microlithic Kernel

TaaviOS uses an experimental kernel architecture I designed called the **Microlithic Kernel**. Kernel model is based on a monolithic kernel with a microlithic tasks like organization.

The kernel is structured around autonomous intelligent kernel clerks. Each clerk has its own field of operations. A filesystem clerk does all filesystem related work, a future network clerk would do all network related. The clerks do not communicate to each other directly so as to minimize the risk of a deadlock. Essentially there should never be horizontal communication between the clerks as why would a filesystem clerk talk with the guy who does landscaping (gui clerk).

Each clerk is like a long-running background service. A task in userspace can make a request that goes directly into clerks mailboxes via syscalls. So the clerk sleep when idle, wake on request, handle the request and wakes the caller. After that the clerks "warms-down" or do other service work related to its own domain. For example fs_task keep up the virtual file (yet to be implemented) all the while the userspace task has continued its stack.

Because the clerk is located on the kernel directory part of page directory it is able to receive the message and communicate results back to it without switching directories.

#### 2. The Blankie Protocol

After all requests and background services have been completed, each kernel clerk performs the **Blankie Protocol**. Think of it as a teleportation device to get the clerk from point B to point A with fresh bottle of water and new shoes.

So the clerk resets its own execution context back to its original entry point with a clean stack, marks itself as *sleeping*, and halts. When new requests arrive the task is woken, it resumes fresh with no leftover stack state from the previous run cycle.

The idea came from continued errors and the want to have an predictable path for the clerks to follow. The protocol is registered at task init time via `blankie_register` and activated via `blankie_activate`.

#### 3. The Hail Mary Protocol

Each kernel clerk that can fault registers can use a **Hail Mary** protocol to recover from a registered callback invoked by the ISR when the clerk causes a fatal exception.

The idea is straightforward: instead of panicking the entire kernel when a kernel task faults, the ISR checks if the faulting task has a registered recovery function and calls it. The clerk gets a second chance; everything else keeps running.

Recovery callbacks are stored in `gosling_table[CLERK_COUNT]`, indexed by PID. Registration is done at init time via `register_hail_mary_function(pid, cb)`. When a fault occurs, `activate_hail_mary(pid)` looks up the table and invokes the callback directly.

The recovery function is responsible for cleaning up any in-flight state, dropping the current request, freeing heap memory and then calling `blankie_activate` to reset the clerk back to its entry point, ready for the next run.


#### 4. Portability

The Blankie and the Microlithic model itself are architecture independent. The current implementation targets x86 but nothing in the design requires it. The concepts port to any architecture with virtual memory and privilege separation.

#### 5. Tradeoffs

All kernel clerks run in ring 0 and share the same address space. A misbehaving clerk can corrupt another clerk's memory. This is the standard monolithic tradeoff. Even Linux has the same risks produced in a different manner. The Hail mary Protocol exists to counter severe bugs at the kernel level, to stop a clerk from shutting down the system

#### 6. Scheduler

The scheduler is a preemptive, priority-aware round-robin design driven by the PIT at 1000Hz.
At each tick, the scheduler considers only tasks that are genuinely ready to run. Tasks with any other state are skipped entirely so to not to consume processor time. The ready queue is the only thing that matters at switch time.

Priority levels exist but do not override fairness within a level. A higher-priority task gets preference when it is ready, but tasks at the same priority rotate equally.

Task cleanup is deliberately separated from the scheduler. A dead task is not destroyed at the moment it exits. It is marked dead and left for a dedicated kernel clerk called the Reaper. Idea is that instead of using processor time to delete tasks the processor can perform another user tasks. 

So we have at least two kernel clerks that are active simultaneously as is user tasks. Where is the asynchrony of it all? Well while the processor advances the userspace stack this tick, the next tick it is killing another, then userspace, then it is closing said userspaces open folder as requested all the while the user task has already continued through it's stack.


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

The parts I have enjoyed most are the ones that required the most original thinking: the scheduler, the Microlithic Kernel model, the Blankie Protocol, and the Ledger Protocol, Hail mary Protocol. Getting preemptive multitasking or the asynchronous open request working for the first time was a particular milestone.

*Author: A.H ~ 2026*