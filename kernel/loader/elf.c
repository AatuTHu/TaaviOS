#include "elf.h"
#include "config.h"
#include "klog.h"
#include "kstring.h"
#if defined(__i686__)
#include "mm.h"
#endif
#include "vmm.h"
#include <stdint.h>

static bool elf_check_file(const Elf_Ehdr *hdr) {
    if (!hdr)
        return false;
    if (hdr->e_ident[EI_MAG0] != ELFMAG0) {
        ERROR("[ELF]: ELF Header EI_MAG0 incorrect.\n");
        return false;
    }
    if (hdr->e_ident[EI_MAG1] != ELFMAG1) {
        ERROR("[ELF]: ELF Header EI_MAG1 incorrect.\n");
        return false;
    }
    if (hdr->e_ident[EI_MAG2] != ELFMAG2) {
        ERROR("[ELF]: ELF Header EI_MAG2 incorrect.\n");
        return false;
    }
    if (hdr->e_ident[EI_MAG3] != ELFMAG3) {
        ERROR("[ELF]: ELF Header EI_MAG3 incorrect.\n");
        return false;
    }
    return true;
}

static bool elf_check_supported(const Elf_Ehdr *hdr) {
    if (!elf_check_file(hdr)) {
        ERROR("[ELF]: Invalid ELF File.\n");
        return false;
    }

    if (hdr->e_ident[EI_CLASS] != ELFCLASS) {
        ERROR("[ELF]: Unsupported ELF File Class.\n");
        return false;
    }

    if (hdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        ERROR("[ELF]: Unsupported ELF File byte order.\n");
        return false;
    }

    if (hdr->e_machine != EM_TYPE) {
        ERROR("[ELF]: Unsupported ELF File target.\n");
        return false;
    }

    if (hdr->e_ident[EI_VERSION] != EV_CURRENT) {
        ERROR("[ELF]: Unsupported ELF File version.\n");
        return false;
    }
    if (hdr->e_type != ET_REL && hdr->e_type != ET_EXEC) {
        ERROR("[ELF]: Unsupported ELF File type.\n");
        return false;
    }

    return true;
}

/**
 * elf_load - turns elf files in to usable tasks.
 * @*program: elf file.
 * @page_dir: directory where to map elf files program.
 * @heap_start_out: first free address after the instructions/program
 *
 * Description:
 * This file takes in .elf formated files. First it checks that the provided program is suitable and
 * supported for the proceeding configuration. Should the program be invalid we return 0 early. 0 is
 * suitable error as the succeed return is the entry point of the program.
 *
 * After that the loop over the entires in the elf header table while casting the provided program to
 * program header by adding to program the headers starting point of elf header + iteration * size of
 * header table entry. If program header is not a type of 1 (PT_LOAD) then we continue to the next header.
 * if it is we calculate the amount of pages it needs, the size and set page flags and allocate the
 * size starting from the program header virtual adddress. Then copy the whole program to the
 * allocated space. After that we calculate the heap start address by adding the program size to
 * program virtual address.
 *
 * Context: Why was it made, when to call it.
 * Return: program entry_point || 0.
 */
uintptr_t elf_load(void *program, page_directory_t *page_dir, uintptr_t *heap_start_out) {
    DEBUG("[ELF]: starting load\n");
    Elf_Ehdr *header = (Elf_Ehdr *)program;

    if (!elf_check_file(header)) {
        return 0;
    }
    if (!elf_check_supported(header)) {
        return 0;
    }

    uintptr_t header_end = 0;

    for (uint16_t i = 0; i < header->e_phnum; i++) {

        Elf_Phdr *phdr = (Elf_Phdr *)((uint8_t *)program + header->e_phoff + i * header->e_phentsize);

        if (phdr->p_type != PT_LOAD)
            continue;
        uintptr_t pages      = (phdr->p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;
        uintptr_t size       = (phdr->p_memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uintptr_t page_flags = PAGE_PRESENT | PAGE_USER;
        if (phdr->p_flags & PF_W) {
            page_flags |= PAGE_RW;
        }
        if (vmm_alloc(page_dir, phdr->p_vaddr, size, page_flags) ==
            STATUS_ERROR) {
            ERROR("[ELF]: virtual memory allocation failed\n");
            return 0;
        }
        for (uintptr_t j = 0; j < pages; j++) {
            uintptr_t file_offset = phdr->p_offset + j * PAGE_SIZE;
            uintptr_t copied      = j * PAGE_SIZE;
            uintptr_t to_copy     = 0;
            if (copied < phdr->p_filesz) {
                to_copy = phdr->p_filesz - copied;
                if (to_copy > PAGE_SIZE)
                    to_copy = PAGE_SIZE;
            }
            uintptr_t to_zero = PAGE_SIZE - to_copy;
            uintptr_t phys    = vmm_get_phys(page_dir, phdr->p_vaddr + j * PAGE_SIZE);
            if (phys == INVALID_PHYSICAL_PAGE) {
                ERROR("[ELF]: Invalid physical page given. Aborting\n");
                return 0;
            }

            memcpy((void *)phys_to_virt(phys), (uint8_t *)program + file_offset,
                   to_copy);
            memset((void *)(phys_to_virt(phys) + to_copy), 0, to_zero);
        }

        uintptr_t segment_end = phdr->p_vaddr + size;
        if (segment_end > header_end) {
            header_end = segment_end;
        }
    }

    if (heap_start_out) {
        *heap_start_out = header_end;
    }

    return header->e_entry;
}
