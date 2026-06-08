#include "elf.h"
#include "config.h"
#include "klog.h"
#include "kstring.h"
#include "mm.h"
#include "pmm.h"
#include "vmm.h"

static bool elf_check_file(const Elf32_Ehdr *hdr) {
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

static bool elf_check_supported(const Elf32_Ehdr *hdr) {
    if (!elf_check_file(hdr)) {
        ERROR("[ELF]: Invalid ELF File.\n");
        return false;
    }
    if (hdr->e_ident[EI_CLASS] != ELFCLASS32) {
        ERROR("[ELF]: Unsupported ELF File Class.\n");
        return false;
    }
    if (hdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        ERROR("[ELF]: Unsupported ELF File byte order.\n");
        return false;
    }
    if (hdr->e_machine != EM_386) {
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

int elf_load(void *data, page_directory_t *page_dir) {

    DEBUG("[ELF]: starting load\n");
    Elf32_Ehdr *header_data = (Elf32_Ehdr *)data;

    if (!elf_check_file(header_data)) {
        return STATUS_ERROR;
    }

    if (!elf_check_supported(header_data)) {
        return STATUS_ERROR;
    }

    for (uint16_t i = 0; i < header_data->e_phnum; i++) {
        Elf32_Phdr *phdr =
            (Elf32_Phdr *)((uint8_t *)data + header_data->e_phoff +
                           i * header_data->e_phentsize);

        if (phdr->p_type != PT_LOAD)
            continue;

        uint32_t pages =
            (phdr->p_memsz + PAGE_SIZE - 1) /
            PAGE_SIZE; // this is the one that calculates and round up the size
        uint32_t size = (phdr->p_memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        uint32_t page_flags = PAGE_PRESENT | PAGE_USER;

        if (phdr->p_flags & PF_W) {
            page_flags |= PAGE_RW;
        }

        if (vmm_alloc(page_dir, phdr->p_vaddr, size, page_flags) ==
            STATUS_ERROR) {
            ERROR("[ELF]: virtual memory allocation failed\n");
            return STATUS_ERROR;
        }

        for (uint32_t j = 0; j < pages; j++) {
            // uint32_t phys = pmm_alloc();
            // DEBUG("[ELF] PT_LOAD vaddr=0x%x filesz=%d memsz=%d\n",
            // phdr->p_vaddr, phdr->p_filesz, phdr->p_memsz);
            // paging_map(page_dir, phdr->p_vaddr + j * PAGE_SIZE, phys,
            // PAGE_USER | PAGE_PRESENT);

            uint32_t file_offset = phdr->p_offset + j * PAGE_SIZE;
            uint32_t copied      = j * PAGE_SIZE;
            uint32_t to_copy     = 0;

            if (copied < phdr->p_filesz) {
                to_copy = phdr->p_filesz - copied;
                if (to_copy > PAGE_SIZE)
                    to_copy = PAGE_SIZE;
            }

            uint32_t to_zero = PAGE_SIZE - to_copy;

            uint32_t phys =
                vmm_get_phys(page_dir, phdr->p_vaddr + j * PAGE_SIZE);
            if (phys == INVALID_PHYSICAL_PAGE) {
                ERROR("[ELF]: Invalid physical page given. Aborting\n");
                return STATUS_ERROR;
            }

            memcpy((void *)phys_to_virt(phys), (uint8_t *)data + file_offset,
                to_copy);
            memset((void *)(phys_to_virt(phys) + to_copy), 0, to_zero);
        }
    }

    return header_data->e_entry;
}