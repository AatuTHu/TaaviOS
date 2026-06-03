#ifndef CONFIG_H
#define CONFIG_H

#define SEG_KERNEL_CODE 0x08
#define SEG_KERNEL_DATA 0x10
#define SEG_USER_CODE 0x1B
#define SEG_USER_DATA 0x23
#define GDT_TSS_SEL 0x2B

#define PIT_FREQUENCY 1000
#define MAX_TASKS 256
#define USER_TASK 1
#define KERNEL_TASK 0

#define HEAP_PAGES 256
#define HEAP_START 0xD0000000

#define PAGE_SIZE 4096
#define MAX_PAGES (1024 * 1024)
#define CONVENTIONAL_MEMORY_KB 1024
#define RESERVED_LOW_PAGES 256

#define VGA_MEMORY_ADDRESS 0xC00B8000
#define VGA_PHYSICAL_ADDRESS 0x000B8000

#define KERNEL_VIRTUAL_BASE 0xC0000000
#define KERNEL_PHYSICAL_ADDRESS 0x00100000
#define KERNEL_VIRTUAL_START (KERNEL_VIRTUAL_BASE + KERNEL_PHYSICAL_ADDRESS)

#define HEAP_MAGIC 0xDEADBEEF

#define STATUS_OK 0
#define STATUS_ERROR -1

#define INVALID_CLUSTER 0
#define INVALID_LBA 0

#define INVALID_PHYSICAL_PAGE 0xFFFFFFFF

typedef enum {
  WRITE,
  READ,
  UPDATE, // this I hear is not a thing with filesystem
  DELETE,
  OPEN
} operations_t;

#define CLERK_COUNT 3 // amount of clerks. Very important. hardcoded for now

#define idle_task_pid 0
#define fs_task_pid 1
#define reaper_task_pid 2

#endif