#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t  jump[3];
    uint8_t  oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t sectors_per_fat;
    uint16_t flags;
    uint16_t fat_version;
    uint32_t root_cluster;
    uint16_t fs_info_sector;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved2;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
} fat32_bpb_t;

typedef struct {
    uint32_t partition_lba;
    uint32_t fat_start;
    uint32_t data_start;
    uint32_t root_cluster;
    uint8_t  sectors_per_cluster;
    uint16_t bytes_per_sector;
    uint8_t  fat_count;
    uint32_t sectors_per_fat;
} fat32_fs_t;

typedef struct __attribute__((packed)) {
    uint8_t  name[8];
    uint8_t  ext[3];
    uint8_t  attributes;
    uint8_t  reserved[8];
    uint16_t cluster_high;
    uint16_t time;
    uint16_t date;
    uint16_t cluster_low;
    uint32_t size;
} fat32_dir_entry_t;

int fat32_init(uint32_t partition_lba);
uint32_t fat32_next_cluster(uint32_t cluster);
int fat32_read_cluster(uint32_t cluster, uint8_t *buf);
void fat32_list_dir(uint32_t cluster);
int fat32_find_file(uint32_t dir_cluster, const char *name, const char *ext, uint32_t *out_cluster, uint32_t *out_size);
int fat32_read_file(uint32_t start_cluster, uint32_t size, uint8_t *buf);
extern fat32_fs_t fat32_fs;
#endif