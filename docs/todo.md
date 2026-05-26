1. sys_exec is not a real function it is a hack and does not use elf
2. sys_write block called if it is not direct vga_write. Eventho that is too incorrect kinda (direct vga write).
3. paging maps vga directly. should be that vga maps via vmm like others
4. vga seems to be poorly written. The scroll atleas
5. vga scroll up and down when arrow press?

6. sys_open, sys_delete(?), sys_read for fs_task mainly
7. fs_task handle requests, mailbox.
8. direct explicisit system_call that can switch to a task that is given