#include "acpi.h"
#include "boot_info.h"
#include "config.h"
#include "klog.h"
#include "kstring.h"
#include <stddef.h>
#include <stdint.h>

struct acpi_info acpi_info;

/**
 * acpi_cheksum - used to verify ACPI table.
 * @*p: points to the table structure.
 * @len: size of the table structure.
 *
 * Description:
 * Verifies that the ACPI table is intact by summing up each byte of the
 * structure. According to the ACPI specification, the total sum of all
 * bytes must wrap around and equal zero.
 *
 * Return: 1 if the checksum is valid (sum equals 0), 0 otherwise.
 */
static int acpi_cheksum(const void *p, size_t len) {
    uint8_t *header_bytes = (uint8_t *)p;
    uint8_t sum           = 0;
    for (size_t i = 0; i < len; i++) {
        sum += header_bytes[i];
    }
    return sum == 0;
}

/**
 * acpi_find_table - used to find a correct structure from the rsdp pointer.
 * @rsdp: root system description pointer.
 * @hhdm: higher half direct map.
 * @sig: target signature.
 *
 * Description:
 * First, this function determines what the next-level root structure is from the RSDP.
 * If the RSDP revision number is 2 or higher and xsdt_addr is not 0, we use the XSDT
 * table as our root. Otherwise, we use the legacy RSDT table.
 *
 * To ensure the root table is intact, we validate its checksum using acpi_cheksum.
 * If valid, we calculate how many entry pointers the root table holds. We loop through
 * these entries, using memcpy to safely extract each entry's physical address (handling
 * potential unaligned memory). We then compute the virtual address using the HHDM,
 * check each table's signature against the target signature, and verify its checksum.
 *
 * * Return: NULL || pointer to the found table structure.
 */
static const struct acpi_sdt_header *acpi_find_table(const struct acpi_rsdp *rsdp,
                                                     uint64_t hhdm,
                                                     const char sig[4]) {
    uint64_t root       = 0;
    uint64_t entry_size = 0;

    if (rsdp->revision >= 2 && rsdp->xsdt_addr != 0) {
        root       = rsdp->xsdt_addr;
        entry_size = 8;
    } else {
        root       = (uint64_t)rsdp->rsdt_addr;
        entry_size = 4;
    }

    const struct acpi_sdt_header *hdr = (const struct acpi_sdt_header *)(root + hhdm);

    if (hdr->length < sizeof(struct acpi_sdt_header)) {
        ERROR("[ACPI]: header length is smaller than the header structure itself. Aborting\n");
        return NULL;
    }

    if (!acpi_cheksum(hdr, hdr->length)) {
        ERROR("[ACPI]: Root header checksum failed.\n");
        return NULL;
    }

    uint64_t entries_count     = (hdr->length - sizeof(struct acpi_sdt_header)) / entry_size;

    const uint8_t *entries_ptr = (const uint8_t *)hdr + sizeof(struct acpi_sdt_header);

    for (uint64_t i = 0; i < entries_count; i++) {
        uint64_t entry_phys = 0;

        if (entry_size == 8) {
            memcpy(&entry_phys, entries_ptr + (i * sizeof(uint64_t)), sizeof(uint64_t));
        } else {
            uint32_t entry32 = 0;
            memcpy(&entry32, entries_ptr + (i * sizeof(uint32_t)), sizeof(uint32_t));
            entry_phys = entry32;
        }

        if (entry_phys == 0) {
            continue;
        }

        const struct acpi_sdt_header *table_header = (const struct acpi_sdt_header *)(entry_phys + hhdm);

        if (table_header->length < sizeof(struct acpi_sdt_header)) {
            continue;
        }

        if (memcmp(table_header->signature, sig, 4) == 0) {
            if (acpi_cheksum(table_header, table_header->length)) {
                return table_header;
            }
        }
    }

    return NULL;
}

/**
 * acpi_sdt_header - .
 * @bi: hold rsdp and hhdm.
 *
 * Description:
 * First this function cheks that bi has rsdp pointer. If it has it casts it to acpi_rsdp struct table pointer (and adds
 * to the pointer the hhdm so that we can use it). Then it compares the signature of the struct table to
 * RSD PTR, valid e the structure by giving the struct to acpi_cheksum so that we know the
 * struct is intact (only the first 20 bytes because we might be dealing with revision 1 struct that
 * is smaller than rev 2 is) and not corrupted. If the revision of the struct is 2 or above we check the
 * whole struct.
 *
 * If all the chekcs are a pass we give the table pointer to acpi_find_table and ask it to find APIC table. If it finds the apic table structure
 * we cast it to madt table so that we can map and save the information of the cpus/timers in the system for
 * later use.
 *
 * Return: NULL || found table structure.
 */

int acpi_init(const struct boot_info *bi) {
    if (bi->rsdp == 0) {
        return STATUS_ERROR;
    }

    struct acpi_rsdp *rsdp = (struct acpi_rsdp *)(bi->rsdp + bi->hhdm_offset);

    if (memcmp(rsdp->signature, "RSD PTR ", 8) != 0) {
        ERROR("[ACPI][INIT]: Invalid RSDP sig\n");
        return STATUS_ERROR;
    }

    if (!acpi_cheksum(rsdp, 20)) {
        ERROR("[ACPI][INIT]: initial rsdp checksum failed\n");
        return STATUS_ERROR;
    }

    DEBUG_KERNEL("[ACPI][INIT]: RSDP Revision: %d\n", rsdp->revision);

    if (rsdp->revision >= 2) {
        if (!acpi_cheksum(rsdp, rsdp->length)) {
            ERROR("[ACPI][INIT]: RSDP extension checksum failed\n");
            return STATUS_ERROR;
        }
    }

    const struct acpi_sdt_header *acpi_header = acpi_find_table(rsdp, bi->hhdm_offset, "APIC");
    if (acpi_header == NULL) {
        ERROR("[ACPI][INIT]: Failed to find MADT table!\n");
        return STATUS_ERROR;
    }

    if (acpi_header->length < sizeof(struct acpi_madt)) {
        ERROR("[ACPI][INIT]: MADT table length %d is too small for struct acpi_madt %d.\n",
              acpi_header->length, sizeof(struct acpi_madt));
        return STATUS_ERROR;
    }

    const struct acpi_madt *madt = (struct acpi_madt *)acpi_header;
    acpi_info.lapic_phys         = madt->lapic_addr;

    DEBUG_KERNEL("[ACPI][INIT]: MADT Flags: 0x%x (PCAT_COMPAT: %d)\n",
                 madt->flags, (madt->flags & 1));

    uint8_t *current = (uint8_t *)madt + sizeof(struct acpi_madt);
    uint8_t *end     = (uint8_t *)acpi_header + acpi_header->length;
    while (current < end) {
        uint8_t type   = current[0];
        uint8_t length = current[1];

        if (length < 2 || current + length > end) {
            break;
        }

        switch (type) {
        case 0: {
            if (length < sizeof(struct acpi_madt_lapic)) {
                break;
            }
            const struct acpi_madt_lapic *cpu = (const struct acpi_madt_lapic *)current;
            if ((cpu->flags & 1) == 0)
                break;
            if (acpi_info.cpu_count >= ACPI_MAX_CPUS)
                break;

            acpi_info.cpus[acpi_info.cpu_count].apic_id = cpu->apic_id;
            acpi_info.cpus[acpi_info.cpu_count].enabled = (cpu->flags & 1);
            acpi_info.cpu_count++;
            break;
        }
        case 1: {
            if (length < sizeof(struct acpi_madt_ioapic)) {
                break;
            }

            const struct acpi_madt_ioapic *ioapic = (const struct acpi_madt_ioapic *)current;
            if (acpi_info.ioapic_count >= ACPI_MAX_IOAPICS)
                break;

            acpi_info.ioapics[acpi_info.ioapic_count].id       = ioapic->id;
            acpi_info.ioapics[acpi_info.ioapic_count].phys     = ioapic->addr;
            acpi_info.ioapics[acpi_info.ioapic_count].gsi_base = ioapic->gsi_base;
            acpi_info.ioapic_count++;
            break;
        }
        case 2: {
            if (length < sizeof(struct acpi_madt_iso)) {
                break;
            }

            const struct acpi_madt_iso *iso = (const struct acpi_madt_iso *)current;
            if (acpi_info.iso_count >= ACPI_MAX_ISOS)
                break;

            acpi_info.isos[acpi_info.iso_count].source_irq = iso->source_irq;
            acpi_info.isos[acpi_info.iso_count].gsi        = iso->gsi;
            acpi_info.isos[acpi_info.iso_count].flags      = iso->flags;
            acpi_info.iso_count++;
            break;
        }
        case 5: {
            if (length < sizeof(struct acpi_madt_lapic_override)) {
                break;
            }

            const struct acpi_madt_lapic_override *lapic_ov = (const struct acpi_madt_lapic_override *)current;
            acpi_info.lapic_phys                            = lapic_ov->lapic_addr;
            break;
        }
        }
        current += length;
    }

    DEBUG_KERNEL("[ACPI][INIT]: Local APIC Physical Address: 0x%x\n", acpi_info.lapic_phys);

    DEBUG_KERNEL("[ACPI][INIT]: Found %d CPUs:\n", acpi_info.cpu_count);
    for (uint32_t i = 0; i < acpi_info.cpu_count; i++) {
        DEBUG_KERNEL("CPU table index %d: APIC ID = %d, Enabled = %d\n",
                     i, acpi_info.cpus[i].apic_id, acpi_info.cpus[i].enabled);
    }

    DEBUG_KERNEL("[ACPI][INIT]: Found %d I/O APICs:\n", acpi_info.ioapic_count);
    for (uint32_t i = 0; i < acpi_info.ioapic_count; i++) {
        DEBUG_KERNEL("IOAPIC table index: %d, id: %d, phys: 0x%x, GSI base: %d\n",
                     i, acpi_info.ioapics[i].id, acpi_info.ioapics[i].phys, acpi_info.ioapics[i].gsi_base);
    }

    DEBUG_KERNEL("[ACPI][INIT]: Found %d Interrupt Source Overrides (ISOs):\n", acpi_info.iso_count);
    for (uint32_t i = 0; i < acpi_info.iso_count; i++) {
        DEBUG_KERNEL("ISO table index: %d, source IRQ: %d, GSI: %d, flags: 0x%x\n",
                     i, acpi_info.isos[i].source_irq, acpi_info.isos[i].gsi, acpi_info.isos[i].flags);
    }

    return STATUS_OK;
}
