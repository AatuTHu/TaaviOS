#ifndef ACPI_H
#define ACPI_H

#include "boot_info.h"
#include <stdint.h>

#define ACPI_MAX_CPUS 64
#define ACPI_MAX_IOAPICS 8
#define ACPI_MAX_ISOS 16

struct acpi_rsdp {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_addr;
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t ext_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

struct acpi_sdt_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

struct acpi_madt {
    struct acpi_sdt_header hdr;
    uint32_t lapic_addr;
    uint32_t flags;
} __attribute__((packed));

struct acpi_madt_entry_hdr {
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

struct acpi_madt_lapic {
    struct acpi_madt_entry_hdr hdr;
    uint8_t acpi_processor_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed));

struct acpi_madt_ioapic {
    struct acpi_madt_entry_hdr hdr;
    uint8_t id;
    uint8_t reserved;
    uint32_t addr;
    uint32_t gsi_base;
} __attribute__((packed));

struct acpi_madt_iso {
    struct acpi_madt_entry_hdr hdr;
    uint8_t bus;
    uint8_t source_irq;
    uint32_t gsi;
    uint16_t flags;
} __attribute__((packed));

struct acpi_madt_lapic_override {
    struct acpi_madt_entry_hdr hdr;
    uint16_t reserved;
    uint64_t lapic_addr;
} __attribute__((packed));

struct acpi_info {
    uint64_t lapic_phys;
    uint32_t cpu_count;
    struct {
        uint8_t apic_id;
        uint8_t enabled;
    } cpus[ACPI_MAX_CPUS];
    uint32_t ioapic_count;
    struct {
        uint8_t id;
        uint32_t phys;
        uint32_t gsi_base;
    } ioapics[ACPI_MAX_IOAPICS];
    uint32_t iso_count;
    struct {
        uint8_t source_irq;
        uint32_t gsi;
        uint16_t flags;
    } isos[ACPI_MAX_ISOS];
};

extern struct acpi_info acpi_info;
int acpi_init(const struct boot_info *bi);

#endif
