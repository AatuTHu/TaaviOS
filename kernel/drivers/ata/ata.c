#include "ata.h"
#include "config.h"
#include "io.h"
#include "klog.h"
#include "stddef.h"
#include <stdint.h>

static ata_drive_t drives[4] = {
    {0x1F0, 0x3F6, 0, 0},
    {0x1F0, 0x3F6, 1, 0},
    {0x170, 0x376, 0, 0},
    {0x170, 0x376, 1, 0},
};
static void _ata_wait_400_ns(ata_drive_t *d) {
    inb(d->ctrl);
    inb(d->ctrl);
    inb(d->ctrl);
    inb(d->ctrl);
}

static int ata_identify(ata_drive_t *d) {
    uint8_t select = d->drive ? 0xB0 : 0xA0;
    outb(d->base + ATA_OFF_DRIVE_HEAD, select);
    _ata_wait_400_ns(d);

    outb(d->base + ATA_OFF_SECTOR_CNT, 0);
    outb(d->base + ATA_OFF_LBA_LOW, 0);
    outb(d->base + ATA_OFF_LBA_MID, 0);
    outb(d->base + ATA_OFF_LBA_HIGH, 0);
    outb(d->base + ATA_OFF_COMMAND, 0xEC);

    uint8_t status = inb(d->base + ATA_OFF_STATUS);
    if (status == 0x00 || status == 0xFF)
        return STATUS_ERROR;

    while (inb(d->base + ATA_OFF_STATUS) & ATA_SR_BSY);

    if (inb(d->base + ATA_OFF_LBA_MID) != 0 ||
        inb(d->base + ATA_OFF_LBA_HIGH) != 0)
        return STATUS_ERROR;

    while (1) {
        status = inb(d->base + ATA_OFF_STATUS);
        if (status & ATA_SR_ERR)
            return STATUS_ERROR;
        if (status & ATA_SR_DRQ)
            break;
    }

    for (int i = 0; i < 256; i++)
        inw(d->base + ATA_OFF_DATA);

    return STATUS_OK;
}

static void ata_wait_ready(ata_drive_t *d) {
    while (inb(d->ctrl) & ATA_SR_BSY);
}

static void ata_reset_wait(ata_drive_t *d) {
    for (volatile int i = 0; i < 100000; i++)
        inb(d->ctrl);
}

static int ata_poll(ata_drive_t *d) {
    _ata_wait_400_ns(d);
    while (1) {
        uint8_t status = inb(d->base + ATA_OFF_STATUS);
        if (status & ATA_SR_ERR)
            return STATUS_ERROR;
        if (status & ATA_SR_DF)
            return STATUS_ERROR;
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ))
            break;
    }
    return STATUS_OK;
}

void ata_init(void) {
    for (int i = 0; i < 4; i++) {
        ata_drive_t *d = &drives[i];
        outb(d->ctrl, 0x04);
        _ata_wait_400_ns(d);
        outb(d->ctrl, 0x00);
        ata_reset_wait(d);

        if (ata_identify(d) == STATUS_OK) {
            d->present = 1;
            DEBUG("[ATA]: Drive %d present\n", i);
        } else {
            DEBUG("[ATA]: Drive %d absent\n", i);
        }
    }
}

int ata_read_sector(ata_drive_t *d, uint32_t lba, uint8_t *buf) {
    uint8_t select = (d->drive ? 0xF0 : 0xE0) | ((lba >> 24) & 0x0F);
    ata_wait_ready(d);
    outb(d->base + ATA_OFF_DRIVE_HEAD, select);
    _ata_wait_400_ns(d);

    outb(d->base + ATA_OFF_SECTOR_CNT, 1);
    outb(d->base + ATA_OFF_LBA_LOW, lba & 0xFF);
    outb(d->base + ATA_OFF_LBA_MID, (lba >> 8) & 0xFF);
    outb(d->base + ATA_OFF_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(d->base + ATA_OFF_COMMAND, ATA_CMD_READ_PIO);

    if (ata_poll(d) == STATUS_ERROR)
        return STATUS_ERROR;

    uint16_t *ptr = (uint16_t *)buf;
    for (int i = 0; i < 256; i++)
        ptr[i] = inw(d->base + ATA_OFF_DATA);

    return STATUS_OK;
}

int ata_write_sector(ata_drive_t *d, uint32_t lba, const uint8_t *buf) {
    uint8_t select = (d->drive ? 0xF0 : 0xE0) | ((lba >> 24) & 0x0F);
    ata_wait_ready(d);
    outb(d->base + ATA_OFF_DRIVE_HEAD, select);
    _ata_wait_400_ns(d);

    outb(d->base + ATA_OFF_SECTOR_CNT, 1);
    outb(d->base + ATA_OFF_LBA_LOW, lba & 0xFF);
    outb(d->base + ATA_OFF_LBA_MID, (lba >> 8) & 0xFF);
    outb(d->base + ATA_OFF_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(d->base + ATA_OFF_COMMAND, ATA_CMD_WRITE_PIO);

    if (ata_poll(d) == STATUS_ERROR)
        return STATUS_ERROR;

    const uint16_t *ptr = (uint16_t *)buf;
    for (int i = 0; i < 256; i++)
        outw(d->base + ATA_OFF_DATA, ptr[i]);

    outb(d->base + ATA_OFF_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait_ready(d);

    return STATUS_OK;
}

ata_drive_t *ata_get_drive(int index) {
    if (index < 0 || index >= 4)
        return NULL;
    if (!drives[index].present)
        return NULL;
    return &drives[index];
}
