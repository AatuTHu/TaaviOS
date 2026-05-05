#ifndef MBR_H
#define MBR_H

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t  status;
    uint8_t  chs_start[3];
    uint8_t  type;
    uint8_t  chs_end[3];
    uint32_t lba_start;
    uint32_t sector_count;
} mbr_partition_t;

typedef struct __attribute__((packed)) {
    uint8_t       bootstrap[446];
    mbr_partition_t partitions[4];
    uint16_t      signature;
} mbr_t;

int mbr_find_fat32(uint32_t *lba_start, uint32_t *sector_count);

#endif