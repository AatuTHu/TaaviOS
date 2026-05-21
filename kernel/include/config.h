#ifndef CONFIG_H
#define CONFIG_H

#define GDT_ENTRIES 6

#define SEG_KERNEL_CODE 0x08
#define SEG_KERNEL_DATA 0x10
#define SEG_USER_CODE   0x1B
#define SEG_USER_DATA   0x23
#define GDT_TSS_SEL     0x2B

/* IDT Attribuutit */
#define IDT_ATTR_INTERRUPT  0x8E // Present, Ring 0, 32-bit Interrupt Gate
#define IDT_ATTR_SYSCALL    0xEE // Present, Ring 3, 32-bit Interrupt Gate

/* PIC Portit ja komennot */
#define PIC1_COMMAND    0x20
#define PIC1_DATA       0x21
#define PIC2_COMMAND    0xA0
#define PIC2_DATA       0xA1
#define PIC_EOI         0x20

/* PIT Portit ja komennot */
#define clock_frequency 1193182
#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL0_PORT 0x40
#define PIT_MODE_SQUARE_WAVE 0x36

/* Prosessien oletusarvot */
#define EFLAGS_DEFAULT_RESERVED 0x002
#define EFLAGS_INTERRUPTS_ENABLED 0x200
#define USER_PROCESS_EFLAGS (EFLAGS_DEFAULT_RESERVED | EFLAGS_INTERRUPTS_ENABLED)

/* GDT Access ja Flags (niille funktioille jotka rakentavat GDT:tä) */
#define GDT_ACCESS_PRESENT   0x80
#define GDT_ACCESS_RING0     0x00
#define GDT_ACCESS_RING3     0x60
#define GDT_ACCESS_CODE_DATA 0x10
#define GDT_ACCESS_EXECUTABLE 0x08
#define GDT_ACCESS_READWRITE 0x02
#define GDT_FLAGS_32BIT      0x04
#define GDT_FLAGS_4K_GRAN    0x08


#define PIT_FREQUENCY 1000
#define MAX_PROCESSES 256
#define USER_PROCESS 1
#define KERNEL_PROCESS 0

#define HEAP_PAGES 256
#define HEAP_START 0xD0000000

/*  */
#define PAGE_SIZE 4096
#define MAX_PAGES (1024 * 1024)
#define CONVENTIONAL_MEMORY_KB  1024
#define RESERVED_LOW_PAGES      256

#define PAGE_PRESENT  (1 << 0)
#define PAGE_RW       (1 << 1)
#define PAGE_USER     (1 << 2)
#define PAGE_USER_RW  (PAGE_PRESENT | PAGE_RW | PAGE_USER) //previous flags in one
#define EFLAGS_IF (1 << 9) //interrupts flag
#define EFLAGS_DEFAULT (EFLAGS_IF | (1 << 1))

#define KERNEL_STACK_SIZE 4096
#define USER_STACK_SIZE (4096 * 4)
#define USER_STACK_TOP  0x800000

#define VGA_MEMORY_ADDRESS 0xC00B8000
#define VGA_PHYSICAL_ADDRESS 0x000B8000

#define KERNEL_VIRTUAL_BASE 0xC0000000
#define KERNEL_PHYSICAL_ADDRESS 0x00100000
#define KERNEL_VIRTUAL_START (KERNEL_VIRTUAL_BASE + KERNEL_PHYSICAL_ADDRESS)

#define HEAP_MAGIC 0xDEADBEEF

#define STATUS_OK       0
#define STATUS_ERROR   -1

#define INVALID_CLUSTER 0
#define INVALID_LBA     0

#define INVALID_PHYSICAL_PAGE 0xFFFFFFFF

#endif