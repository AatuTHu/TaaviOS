#include "mbr.h"
#include "ata.h"
#include "config.h"
#include "klog.h"

int mbr_find_fat32(ata_drive_t *drive, uint32_t *lba_start, uint32_t *sector_count) {
    char buf[512];

    if (ata_read_sector(drive, 0, (uint8_t *)buf) != 0) {
        ERROR("[MBR][FIND_FAT32]: Failed to read sector\n");
        return STATUS_ERROR;
    }
    mbr_t *mbr = (mbr_t *)buf;

    if (mbr->signature != 0xAA55) {
        ERROR("[MBR][FINT_FAT32]: Invalid siganture aborting\n");
        return STATUS_ERROR;
    }

    for (int i = 0; i < 4; i++) {
        if (mbr->partitions[i].type == 0x0B ||
            mbr->partitions[i].type == 0x0C) {
            *lba_start    = mbr->partitions[i].lba_start;
            *sector_count = mbr->partitions[i].sector_count;
            return STATUS_OK;
        }
    }

    ERROR("[MBR][FINT_FAT32]: No mbr partition matched the type 0x0B or 0x0C\n");
    return STATUS_ERROR;
}
