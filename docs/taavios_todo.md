Taavi OS — TODO

Ledger Protocol

Cursor fragmentation — freed slots get reused via size==0 scan but cursor never rewinds. Acceptable for now but will fragment protocol address space over time. Revisit when sys_close is built. 
 -> retired & replaced with hail mary protocol. Still thingking a way of using this later.

Kernel / Syscall

sys_exec is a hack — no ELF loading, needs a real implementation -> store entry to filesystem and read it at runtime?

VGA

VGA is mapped directly in paging. Should go through VMM like all other memory
VGA driver is poorly written -> scroll is broken or missing
Scroll up/down on arrow key press

Filesystem

sys_open -> working
sys_read -> working
sys_write -> working
sys_delete -> not started
sys_close -> not started

fs_task request handle -> expand for write and delete operations

