#ifndef FAT32_H
#define FAT32_H
#include <stdint.h>

/* Sector and FAT geometry */
#define FAT32_SECTOR_SIZE        512
#define FAT32_SECTOR_MASK        (FAT32_SECTOR_SIZE - 1)
#define FAT32_FAT_ENTRY_SIZE     4
#define FAT32_ENTRIES_PER_SECTOR (FAT32_SECTOR_SIZE / FAT32_FAT_ENTRY_SIZE)
#define FAT32_DIRENT_SIZE        32

/* Cluster sentinel values */
#define FAT32_CLUSTER_FREE       0x00000000
#define FAT32_CLUSTER_BAD        0x0FFFFFF7
#define FAT32_CLUSTER_EOC        0x0FFFFFFF
#define FAT32_CLUSTER_EOC_MIN    0x0FFFFFF8
#define FAT32_CLUSTER_MASK       0x0FFFFFFF

/* Directory entry first-byte markers */
#define FAT32_DIRENT_FREE        0x00
#define FAT32_DIRENT_DELETED     0xE5

/* Attribute flags */
#define FAT32_ATTR_READ_ONLY     0x01
#define FAT32_ATTR_HIDDEN        0x02
#define FAT32_ATTR_SYSTEM        0x04
#define FAT32_ATTR_VOLUME_ID     0x08
#define FAT32_ATTR_DIRECTORY     0x10
#define FAT32_ATTR_ARCHIVE       0x20
#define FAT32_ATTR_LFN           0x0F

/* 8.3 formatting */
#define FAT32_83_PAD             0x20
#define ASCII_CASE_DIFF          0x20

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
    uint32_t maximum_cluster_size;
    uint32_t last_allocated_cluster;
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
} fat32_dirent_t;

/* Read */
int      fat32_init(uint32_t partition_lba);
uint32_t fat32_next_cluster(uint32_t cluster);
int      fat32_read_cluster(uint32_t cluster, uint8_t *buf);
void     fat32_list_dir(uint32_t cluster);
int      fat32_find_file(uint32_t dir_cluster, const char *name, const char *ext, uint32_t *out_cluster, uint32_t *out_size);
int      fat32_read_file(uint32_t start_cluster, uint32_t size, uint8_t *buf);

/* Write */
int      fat32_write_cluster(uint32_t cluster, const uint8_t *buf);
uint32_t fat32_alloc_cluster(void);
int      fat32_set_cluster(uint32_t cluster, uint32_t value);
uint32_t fat32_write_file(const uint8_t *buf, uint32_t size);
int      fat32_format_83(const char *filename, uint8_t *dst);
int      fat32_create_dirent(uint32_t dir_cluster, const char *filename, uint32_t first_cluster, uint32_t size);

extern fat32_fs_t f32_fs;

#endif