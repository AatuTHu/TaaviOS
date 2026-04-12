#include "elf.h"
#include "vmm.h"
#include "mm.h"
#include "pmm.h"
#include "kstring.h"
#include "config.h"
#include "klog.h"

bool elf_check_file(Elf32_Ehdr *hdr) {
	if(!hdr) return false;
	if(hdr->e_ident[EI_MAG0] != ELFMAG0) {
		DEBUG("ELF Header EI_MAG0 incorrect.\n");
		return false;
	}
	if(hdr->e_ident[EI_MAG1] != ELFMAG1) {
		DEBUG("ELF Header EI_MAG1 incorrect.\n");
		return false;
	}
	if(hdr->e_ident[EI_MAG2] != ELFMAG2) {
		DEBUG("ELF Header EI_MAG2 incorrect.\n");
		return false;
	}
	if(hdr->e_ident[EI_MAG3] != ELFMAG3) {
		DEBUG("ELF Header EI_MAG3 incorrect.\n");
		return false;
	}
	return true;
}

bool elf_check_supported(Elf32_Ehdr *hdr) {
	if(!elf_check_file(hdr)) {
		DEBUG("Invalid ELF File.\n");
		return false;
	}
	if(hdr->e_ident[EI_CLASS] != ELFCLASS32) {
		DEBUG("Unsupported ELF File Class.\n");
		return false;
	}
	if(hdr->e_ident[EI_DATA] != ELFDATA2LSB) {
		DEBUG("Unsupported ELF File byte order.\n");
		return false;
	}
	if(hdr->e_machine != EM_386) {
		DEBUG("Unsupported ELF File target.\n");
		return false;
	}
	if(hdr->e_ident[EI_VERSION] != EV_CURRENT) {
		DEBUG("Unsupported ELF File version.\n");
		return false;
	}
	if(hdr->e_type != ET_REL && hdr->e_type != ET_EXEC) {
		DEBUG("Unsupported ELF File type.\n");
		return false;
	}
	return true;
}

uint32_t elf_load(void *data, page_directory_t *page_dir) {

    Elf32_Ehdr *header_data = (Elf32_Ehdr *) data;

    if(!elf_check_file(header_data)) {     
        return 0;
    }

    if(!elf_check_supported(header_data)) {
        return 0;
    }

    for(uint16_t i = 0; i < header_data->e_phnum; i++) {
        Elf32_Phdr *phdr = (Elf32_Phdr *)((uint8_t*)data + header_data->e_phoff + i * header_data->e_phentsize);

        if(phdr->p_type != PT_LOAD) continue;

        uint32_t pages = (phdr->p_memsz + PAGE_SIZE-1) / PAGE_SIZE;

        for(uint32_t j = 0; j < pages; j++) {
            uint32_t phys = pmm_alloc();

            paging_map(page_dir, phdr->p_vaddr + j * PAGE_SIZE, phys, PAGE_USER_RW);

            uint32_t file_offset = phdr->p_offset + j * PAGE_SIZE;
            uint32_t copied = j * PAGE_SIZE;
            uint32_t to_copy = 0;

            if(copied < phdr->p_filesz) {
                to_copy = phdr->p_filesz - copied;
                if(to_copy > PAGE_SIZE) to_copy = PAGE_SIZE;
            }

            uint32_t to_zero = PAGE_SIZE - to_copy;

            memcpy((void*)phys_to_virt(phys), (uint8_t*)data + file_offset, to_copy);
            memset((void*)(phys_to_virt(phys) + to_copy), 0, to_zero);
        }
    }

    return header_data->e_entry;
}