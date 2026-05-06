#include "mbr.h"
#include "ata.h"
#include "klog.h"

int mbr_find_fat32(uint32_t *lba_start, uint32_t *sector_count) {
    char buf[512];

    if (ata_read_sector(0, (uint8_t *)buf) != 0) return -1;
    mbr_t *mbr = (mbr_t *)buf;
    
    if(mbr->signature != 0xAA55) return -1;

    for(int i = 0; i < 4; i++) {
        if (mbr->partitions[i].type == 0x0B || mbr->partitions[i].type == 0x0C) {
            *lba_start = mbr->partitions[i].lba_start;
            *sector_count = mbr->partitions[i].sector_count;
            return 0;
        }
    }

    return -1;
}