# Carrots - 32bit x86 operating system

### Yet another x86 operating system?

Yes.

### Why?

Yes.

### Does it have anything special?

#### 1. Microlithic Kernel & The Blankie Protocol

Well for starters, a kernel architecture designed by me. Well, others have done it for sure, but the gist is that the kernel has tasks similar to userspace, but they do share a single page directory. You might now wonder what stops a bug in one task from corrupting another. The answer is **trying** a strict software-level isolation. Each kernel task operates as its own police state. Before doing anything critical, the task actively checks and validates its own allocated memory boundaries.

The idea of this kernel type came from backend and frontend web framework paradigms. I wanted the kernel to be an asynchronous servant of the user.

Since all kernel tasks share the same memory space, the usertasks can contact them without unnecessary context switches.

Some would say that this is just and abstraction layer over the drivers and whatnot. To that I say: that it is. I have made complicated simplicity.

#### What does kernel task do then?

A task like *fs_task*s (short for filesystem_task) job is to take in requests from userspace applications. When a request is made, the task wakes up from sleep state and places itself as high priority. These requests are basically CRUD operations for the filesystem. So it does as requested and returns the result to the caller. After that, the caller can continue its life, but here is the catch: The *fs_task* continues to do other services for the filesystem. After requests are done the task can do some background work like:

**these are a work in progress still**

- Open the connection for filesystem. Warm it up for usage.

- Keeping a virtual */sys_info* file updated.

- Delayed disk writes: When an app saves data, *fs_task* doesn't write it to the disk immediately. It dumps the data into RAM first so the app can continue instantly, and handles the actual slow disk writing later as a background service before going to sleep.

- Cleaning up and compacting directory caches.

Other tasks I have in mind to implement this way are a networker and a GUI. To keep the code clean and avoid locking hell, these kernel tasks never talk to each other horizontally. The userspace app acts as the conductor, making vertical requests to each task independently.

Then, after there are no more requests or services left, each kernel task performs what I call the "Blankie Protocol"—just a fancy name for being blocked. They place themselves to a low priority as well as switching their state to sleeping. This is important distinction from blocked.

Design Decisions & Tradeoffs: 
- No IPC overhead: Kernel tasks share address space. Messages are dropped into task queues instantly during system calls (like sys_write) without unnecessary context switches.
- Isolation without virtualization: Software-level validation inside the tasks, instead of relying solely on expensive hardware page table swaps for internal jobs.
- No privilege separation inside Ring 0: A severe bug in fs_task can corrupt other kernel memory if the software-level borders fail.

I call it a Microlithic kernel. In my mind, it combines the microkernel design and the monolith as everything important is in the kernel space, but those important things are isolated into independent tasks.

## 2. Opportunistic Scheduler

An opportunistic round-robin, priority combined with states first-find-selected design. It's opportunistic in a way that if a standard task is blocked or dead, the scheduler picks up those skipped CPU cycles and redirects them to internal housekeeping. If they weren't picked, that time would be wasted, but now the system always has something useful to do.

## 3. The "Learn as You Go" Engineer type

I have an engineering degree in Information and Communications Technology, but my background is in making web/mobile apps using JS and React. Before this project, I had no real prior experience in low-level C or NASM, and nothing to do with OS development.

I learn concepts by reading other people's work, books, the OSDev wiki, YouTube videos, and using free versions of AI to explain abstract concepts and bounce pseudo-code around.

I have mostly enjoyed the making and designing the scheduler and the *"microlithic"* kernel type model. 

### Current Status

* Arch: 32-bit Protected Mode (Flat model using Paging).
* Working memory management.
* Multitasking: Preemptive, task-isolated architecture.
* userspace
* small c libs

Author: A.H ~ 27.05.2026