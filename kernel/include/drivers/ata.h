#ifndef ATA_H
#define ATA_H

#include <stdint.h>

#define ATA_DATA 0x1F0
#define ATA_ERROR 0x1F1
#define ATA_FEATURES 0x1F1
#define ATA_SECTOR_CNT 0x1F2
#define ATA_LBA_LOW 0x1F3
#define ATA_LBA_MID 0x1F4
#define ATA_LBA_HIGH 0x1F5
#define ATA_DRIVE_HEAD 0x1F6
#define ATA_STATUS 0x1F7
#define ATA_COMMAND 0x1F7
#define ATA_ALT_STATUS 0x3F6

#define ATA_SR_BSY 0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DF 0x20
#define ATA_SR_DRQ 0x08
#define ATA_SR_ERR 0x01
#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_WRITE_PIO 0x30

#define ATA_OFF_DATA 0x00
#define ATA_OFF_SECTOR_CNT 0x02
#define ATA_OFF_LBA_LOW 0x03
#define ATA_OFF_LBA_MID 0x04
#define ATA_OFF_LBA_HIGH 0x05
#define ATA_OFF_DRIVE_HEAD 0x06
#define ATA_OFF_STATUS 0x07
#define ATA_OFF_COMMAND 0x07

typedef struct {
    uint16_t base;
    uint16_t ctrl;
    uint8_t drive;
    uint8_t present;
} ata_drive_t;

ata_drive_t *ata_get_drive(int index);
int ata_read_sector(ata_drive_t *drive, uint32_t lba, uint8_t *buf);
int ata_write_sector(ata_drive_t *drive, uint32_t lba, const uint8_t *buf);
void ata_init(void);

#endif
