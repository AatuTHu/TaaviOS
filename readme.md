# Carrots - 32bit x86 operating system

### Yet another x86 operating system?

Yes.

### Why?

Yes.

### Does it have anything special?

#### 1. Microlithic Kernel & The Blankie Protocol

A kernel architecture designed by me. Well, others have done it for sure, but the gist is that the kernel has tasks similar to userspace, but they share a single page directory. You might now wonder what stops a bug in one task from corrupting another. The answer is **trying** a strict software-level isolation. Each kernel task operates as its own police state. Before doing anything critical, the task actively checks and validates its own allocated memory boundaries.

The idea of this kernel type came from backend and frontend web framework paradigms. I wanted the kernel to be an asynchronous servant of the user.

Since all kernel tasks share the same memory space, the usertasks can contact them without unnecessary context switches.

Some would say that this is just an abstraction layer over the drivers and whatnot. To that I say: that it is. I have made **complicated simplicity**.

#### What does a kernel task do?

A task like *fs_task* (short for filesystem_task) takes in requests from userspace applications. When a request is made, the task wakes up from sleep state and places itself as high priority. These requests are basically CRUD operations for the filesystem. It does as requested and returns the result to the caller. After that, the caller can continue its life — but here is the catch: *fs_task* continues to do other services for the filesystem. After requests are done the task can do background work like:

**these are a work in progress still**

- Opening the connection for the filesystem. Warming it up for usage.
- Keeping a virtual */sys_info* file updated.
- Delayed disk writes: When an app saves data, *fs_task* doesn't write to disk immediately. It dumps the data into RAM first so the app can continue instantly, and handles the actual slow disk write later as a background service.
- Cleaning up and compacting directory caches.

Other tasks planned for this model: a GUI task and a network task. To keep the code clean and avoid locking hell, kernel tasks never talk to each other horizontally. The userspace app acts as the conductor, making vertical requests to each task independently.

#### The Blankie Protocol

After all requests are served and background services are complete, each kernel task performs what I call the **Blankie Protocol**. The task resets its own execution context back to its entry point with a clean stack, marks itself as sleeping, and halts. The scheduler ignores sleeping tasks entirely. When new work arrives and the task is woken, it resumes fresh — as if it just started — with no leftover state or stack corruption from the previous run cycle.

This is an important distinction from blocked. Blocked means waiting for something. Sleeping means done and intentionally dormant.

The protocol is registered at task init time via `blankie_register` and activated via `blankie_activate`. Any kernel task gets it for free.

#### Design Decisions & Tradeoffs

- No IPC overhead: Kernel tasks share address space. Messages are dropped into task queues instantly during system calls without unnecessary context switches.
- Isolation without virtualization: Software-level validation inside the tasks, instead of relying solely on expensive hardware page table swaps for internal jobs.
- No privilege separation inside Ring 0: A severe bug in a kernel task can corrupt other kernel memory if the software-level borders fail. While memory corruption is a risk in any kernel, the shared address space makes it a higher likelihood here. It is an accepted tradeoff.

I call it a **Microlithic kernel**. Monolithic in that everything important lives in kernel space. Microkernel-inspired in that those important things are organized into independent, domain-owning tasks.

#### 2. Scheduler

The scheduler is a round-robin, priority-aware design. It only considers tasks that are ready to run — dead, blocked, and sleeping tasks are simply skipped. Task cleanup is intentionally separated from scheduling, keeping the context switch logic focused and predictable.


#### 3. The "Learn as You Go" Engineer

I have an engineering degree in Information and Communications Technology, but my background is making web and mobile apps using JS and React. Before this project I had no real prior experience in low-level C or NASM, and nothing to do with OS development.

I learn by reading other people's work, books, the OSDev wiki, YouTube videos, and using AI to explain abstract concepts and bounce ideas around.

I have mostly enjoyed designing the scheduler and the microlithic kernel model.


### Current Status

- Arch: 32-bit Protected Mode, higher-half kernel, paging enabled
- Working memory management: PMM, VMM, kmalloc
- Preemptive multitasking with task-isolated address spaces
- Microlithic kernel with working fs_task and blankie protocol
- FAT32 read/write driver
- Userspace with shell
- Small C userspace library

*Author: A.H ~ 2026*