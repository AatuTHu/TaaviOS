KERNEL_VIRTUAL_BASE equ 0xC0000000
KERNEL_PAGE_DIR_IDX equ (KERNEL_VIRTUAL_BASE >> 22)

; ─────────────────────────────────────────────
; Multiboot header
; ─────────────────────────────────────────────
section .multiboot
align 4

MULTIBOOT_MAGIC     equ 0x1BADB002
MULTIBOOT_FLAGS     equ 0x07
MULTIBOOT_CHECKSUM  equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)
mode_type           equ 0
width               equ 800
height              equ 600
depth               equ 32

dd MULTIBOOT_MAGIC
dd MULTIBOOT_FLAGS
dd MULTIBOOT_CHECKSUM
dd 0 ; header_addr
dd 0 ; load_addr
dd 0 ; load_end_addr
dd 0 ; bss_end_addr
dd 0 ; entry_addr
dd mode_type
dd width
dd height
dd depth


; ─────────────────────────────────────────────
; Early boot stack (physical, used before higher-half jump)
; ─────────────────────────────────────────────
section .bss
align 16

boot_stack_bottom:
resb 16384          ; 16KB early stack
boot_stack_top:

; ─────────────────────────────────────────────
; Page directory (must be 4KB aligned)
; ─────────────────────────────────────────────
align 4096
boot_page_dir:
resd 1024           ; 1024 x 4-byte entries, all zeroed

; ─────────────────────────────────────────────
; Boot entry — GRUB jumps here
; ─────────────────────────────────────────────
section .text
global boot_entry
global boot_stack_top
boot_entry:

mov edx, ebx
mov esp, (boot_stack_top - KERNEL_VIRTUAL_BASE)
mov dword [boot_page_dir - KERNEL_VIRTUAL_BASE], 0x83
mov dword [boot_page_dir - KERNEL_VIRTUAL_BASE + 3072], 0x83



mov ecx, cr4
OR ecx, 0x10
mov cr4, ecx
mov ecx, (boot_page_dir - 0xC0000000)
mov cr3, ecx
mov ecx, cr0
OR ecx, 0x80000000
mov cr0, ecx


mov ecx, higher_half
jmp ecx

higher_half:

mov dword [boot_page_dir], 0

mov ecx, cr3
mov cr3, ecx


mov esp, boot_stack_top
extern kernel_main
push edx

call kernel_main

cli
.hang:
hlt
jmp .hang