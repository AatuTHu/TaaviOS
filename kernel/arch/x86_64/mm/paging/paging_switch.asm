[bits 64]
[GLOBAL switch_page_dir]

switch_page_dir:
mov cr3, rdi
ret
