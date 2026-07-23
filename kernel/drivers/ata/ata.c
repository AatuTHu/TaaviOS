#include "ata.h"
#include "config.h"
#include "io.h"
#include "klog.h"

static void _ata_wait_400_ns() {
    inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS);
}

static void ata_wait_ready(void) {
    while (inb(ATA_STATUS) & ATA_SR_BSY);
}

static int ata_poll(void) {
    _ata_wait_400_ns();
    while (1) {
        uint8_t status = inb(ATA_STATUS);
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
    outb(ATA_ALT_STATUS, 0x04);
    outb(ATA_ALT_STATUS, 0x00);
    DEBUG("[ATA]: ..Polling..\n");
    ata_wait_ready();
    DEBUG("[ATA]: Ata ready\n");
}

int ata_read_sector(uint32_t lba, uint8_t *buf) {
    ata_wait_ready();
    outb(ATA_DRIVE_HEAD, 0xF0 | ((lba >> 24) & 0x0F));

    _ata_wait_400_ns();

    outb(ATA_SECTOR_CNT, 1);
    outb(ATA_LBA_LOW, lba & 0xFF);
    outb(ATA_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(ATA_COMMAND, ATA_CMD_READ_PIO);

    if (ata_poll() == STATUS_ERROR)
        return STATUS_ERROR;

    uint16_t *ptr = (uint16_t *)buf;
    for (int i = 0; i < 256; i++) {
        ptr[i] = inw(ATA_DATA);
    }
    return STATUS_OK;
}

int ata_write_sector(uint32_t lba, const uint8_t *buf) {
    ata_wait_ready();
    outb(ATA_DRIVE_HEAD, 0xF0 | ((lba >> 24) & 0x0F));

    outb(ATA_SECTOR_CNT, 1);
    outb(ATA_LBA_LOW, lba & 0xFF);
    outb(ATA_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(ATA_COMMAND, ATA_CMD_WRITE_PIO);

    if (ata_poll() == STATUS_ERROR)
        return STATUS_ERROR;

    const uint16_t *ptr = (uint16_t *)buf;
    for (int i = 0; i < 256; i++) {
        outw(ATA_DATA, ptr[i]);
    }

    outb(ATA_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait_ready();

    return STATUS_OK;
}
