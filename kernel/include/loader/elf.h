#ifndef ELF_H
#define ELF_H

#include "paging.h"
#include <stdbool.h>
#include <stdint.h>
#define ELF_NIDENT 16

enum Elf_Ident {
    EI_MAG0       = 0, // 0x7F
    EI_MAG1       = 1, // 'E'
    EI_MAG2       = 2, // 'L'
    EI_MAG3       = 3, // 'F'
    EI_CLASS      = 4, // Architecture (32/64)
    EI_DATA       = 5, // Byte Order
    EI_VERSION    = 6, // ELF Version
    EI_OSABI      = 7, // OS Specific
    EI_ABIVERSION = 8, // OS Specific
    EI_PAD        = 9  // Padding
};

enum Elf_Type {
    ET_NONE = 0, // Unkown Type
    ET_REL  = 1, // Relocatable File
    ET_EXEC = 2, // Executable File
    ET_DYN  = 3  // PIE
};

#define EV_CURRENT (1)
#define ELFMAG0 0x7F // e_ident[EI_MAG0]
#define ELFMAG1 'E'  // e_ident[EI_MAG1]
#define ELFMAG2 'L'  // e_ident[EI_MAG2]
#define ELFMAG3 'F'  // e_ident[EI_MAG3]
#define PT_LOAD 1

#define PF_X (1 << 0) // Execute
#define PF_W (1 << 1) // Write
#define PF_R (1 << 2) // Read

#define ELFDATA2LSB (1)    // Little Endian
typedef uint16_t Elf_Half; // Unsigned half int
typedef uint32_t Elf_Word; // Unsigned int
typedef int32_t Elf_Sword; // Signed int

#if defined(__x86_64__)
#define EM_TYPE (62)       // -..-
#define ELFCLASS (2)       // 32*2 bit arch
typedef uint64_t Elf_Addr; // Unsigned address
typedef uint64_t Elf_Off;  // Unsigned offset
typedef uint64_t Elf_Xword;
typedef struct {
    uint8_t e_ident[ELF_NIDENT];
    Elf_Half e_type;
    Elf_Half e_machine;
    Elf_Word e_version;
    Elf_Addr e_entry;
    Elf_Off e_phoff;
    Elf_Off e_shoff;
    Elf_Word e_flags;
    Elf_Half e_ehsize;
    Elf_Half e_phentsize;
    Elf_Half e_phnum;
    Elf_Half e_shentsize;
    Elf_Half e_shnum;
    Elf_Half e_shstrndx;
} Elf_Ehdr;

typedef struct {
    Elf_Word p_type;
    Elf_Word p_flags;
    Elf_Off p_offset;
    Elf_Addr p_vaddr;
    Elf_Addr p_paddr;
    Elf_Xword p_filesz;
    Elf_Xword p_memsz;
    Elf_Xword p_align;
} Elf_Phdr;
#else
#define EM_TYPE (3)  // x86 Machine Type
#define ELFCLASS (1) // 32-bit Architecture
typedef uint32_t Elf_Off;  // Unsigned offset
typedef uint32_t Elf_Addr; // Unsigned address
typedef struct {
    uint8_t e_ident[ELF_NIDENT];
    Elf_Half e_type;
    Elf_Half e_machine;
    Elf_Word e_version;
    Elf_Addr e_entry;
    Elf_Off e_phoff;
    Elf_Off e_shoff;
    Elf_Word e_flags;
    Elf_Half e_ehsize;
    Elf_Half e_phentsize;
    Elf_Half e_phnum;
    Elf_Half e_shentsize;
    Elf_Half e_shnum;
    Elf_Half e_shstrndx;
} Elf_Ehdr;

typedef struct {
    Elf_Word p_type;
    Elf_Off p_offset;
    Elf_Addr p_vaddr;
    Elf_Addr p_paddr;
    Elf_Word p_filesz;
    Elf_Word p_memsz;
    Elf_Word p_flags;
    Elf_Word p_align;
} Elf_Phdr;
#endif

uintptr_t elf_load(void *program, page_directory_t *page_dir, uintptr_t *heap_start_out);

#endif
