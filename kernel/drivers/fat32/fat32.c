#include "fat32.h"
#include "ata.h"
#include "klog.h"
#include "kstring.h"

fat32_fs_t f32_fs;

int fat32_init(uint32_t partition_lba) {
    char buf[FAT32_SECTOR_SIZE];
    ata_read_sector(partition_lba, buf);
    fat32_bpb_t *bpb = (fat32_bpb_t *)buf;
    
    if(bpb->bytes_per_sector != FAT32_SECTOR_SIZE) {
        DEBUG("[FAT32]: bytes_pre_sector lower than 512. Is: %d\n", bpb->bytes_per_sector);
    }

    f32_fs.partition_lba          = partition_lba;
    f32_fs.fat_start              = partition_lba + bpb->reserved_sectors;
    f32_fs.data_start             = f32_fs.fat_start + (bpb->fat_count * bpb->sectors_per_fat);
    f32_fs.root_cluster           = bpb->root_cluster;
    f32_fs.sectors_per_cluster    = bpb->sectors_per_cluster;
    f32_fs.bytes_per_sector       = bpb->bytes_per_sector;
    f32_fs.fat_count              = bpb->fat_count;
    f32_fs.sectors_per_fat        = bpb->sectors_per_fat;

    DEBUG("[FAT32]: partition_lba: %d\n", f32_fs.partition_lba);
    DEBUG("[FAT32]: fat_start: %d\n", f32_fs.fat_start);
    DEBUG("[FAT32]: data_start: %d\n", f32_fs.data_start);
    DEBUG("[FAT32]: root_cluster: %d\n", f32_fs.root_cluster);
    DEBUG("[FAT32]: sectors_per_cluster: %d\n", f32_fs.sectors_per_cluster);
    DEBUG("[FAT32]: bytes_pre_sector: %d\n", f32_fs.bytes_per_sector);
    DEBUG("[FAT32]: fat_count: %d\n", f32_fs.fat_count);
    DEBUG("[FAT32]: sector_per_fat %d\n", f32_fs.sectors_per_fat);
    
    return 0;
}

uint32_t fat32_next_cluster(uint32_t cluster) {
    uint32_t fat_sector = f32_fs.fat_start + (cluster * FAT32_FAT_ENTRY_SIZE) / f32_fs.bytes_per_sector;
    uint32_t fat_offset = (cluster * FAT32_FAT_ENTRY_SIZE) % f32_fs.bytes_per_sector;
    char buf[FAT32_SECTOR_SIZE];
    
    ata_read_sector(fat_sector, buf);
    uint32_t *ptr = (uint32_t*) buf; 
    uint32_t entry = ptr[fat_offset / FAT32_FAT_ENTRY_SIZE];

    return (entry & FAT32_CLUSTER_MASK);
}

int fat32_read_cluster(uint32_t cluster, uint8_t *buf) {
    uint32_t cluster_sector = f32_fs.data_start + (cluster - 2) * f32_fs.sectors_per_cluster;

    for(uint8_t i = 0; i < f32_fs.sectors_per_cluster; i++) {
        if(ata_read_sector((cluster_sector + i), (buf + i * FAT32_SECTOR_SIZE)) == -1) {
            DEBUG("[FAT32]: reading clusters went wrong. Aborting \n");
            return -1;
        }
    }
    return 0;
}

void fat32_list_dir(uint32_t cluster) {
    uint8_t buf[FAT32_SECTOR_SIZE];

    fat32_read_cluster(cluster, buf);

    fat32_dirent_t *dir_entry = (fat32_dirent_t *)buf;

    for(int i = 0; i < (f32_fs.sectors_per_cluster * FAT32_SECTOR_SIZE) / FAT32_DIRENT_SIZE; i++) {
        if(dir_entry[i].name[0] == FAT32_DIRENT_FREE) break;
        if(dir_entry[i].name[0] == FAT32_DIRENT_DELETED) continue;
        if ((dir_entry[i].attributes & 0x0F) == 0x0F) continue;
        DEBUG("[FAT32]: name: %.8s.%.3s\n", dir_entry[i].name, dir_entry[i].ext);
        DEBUG("[FAT32]: dir size: %d\n", dir_entry[i].size);
    }

    DEBUG("[FAT32]: searching for more clusters\n");
    uint32_t next_cluster = fat32_next_cluster(cluster);
    if(next_cluster >= FAT32_CLUSTER_EOC_MIN) return;

    DEBUG("[FAT32]: More clusters found at %d\n", next_cluster);
    fat32_list_dir(next_cluster);
}

int fat32_read_file(uint32_t start_cluster, uint32_t size, uint8_t *buf) {

    uint32_t bytes_read = 0;
    uint32_t cluster_size = f32_fs.sectors_per_cluster * FAT32_SECTOR_SIZE;
    uint32_t current_cluster = start_cluster;
    while(1) {
        fat32_read_cluster(current_cluster, buf + bytes_read);
        bytes_read += cluster_size;
        
        if(bytes_read >= size) break;
        uint32_t next = fat32_next_cluster(current_cluster);
        if(next >= FAT32_CLUSTER_EOC_MIN) break;
        current_cluster = next;
    }

    return 0;
}

int fat32_find_file(uint32_t dir_cluster, const char *name, const char *ext, uint32_t *out_cluster, uint32_t *out_size) {
    char buf[FAT32_SECTOR_SIZE];
    uint32_t current = dir_cluster;
    uint32_t cluster_size = f32_fs.sectors_per_cluster * FAT32_SECTOR_SIZE;

    while(1) {
        fat32_read_cluster(current, buf);
        fat32_dirent_t* dir_entry = (fat32_dirent_t*)buf;

        for(int i = 0; i < (cluster_size / FAT32_DIRENT_SIZE); i++) {
            if(dir_entry[i].name[0] == FAT32_DIRENT_FREE) break;
            if(dir_entry[i].name[0] == FAT32_DIRENT_DELETED) continue;
            if ((dir_entry[i].attributes & 0x0F) == 0x0F) continue;
            if(memcmp(dir_entry[i].name, name, 8) == 0 && memcmp(dir_entry[i].ext,  ext, 3) == 0) {
                *out_cluster = (dir_entry[i].cluster_high << 16) | dir_entry[i].cluster_low;
                *out_size = dir_entry[i].size;
                return 0;
            }
        }
        uint32_t next_cluster = fat32_next_cluster(current);
        if(next_cluster >= FAT32_CLUSTER_EOC_MIN) return -1;
        current = next_cluster;
    }
    
    return -1;
}

uint32_t fat32_alloc_cluster(void) {
    uint8_t buf[FAT32_SECTOR_SIZE];

    for(int i = 0; i < f32_fs.sectors_per_fat; i++) {
        if(ata_read_sector((f32_fs.fat_start + i), buf) == -1) return 0;    
        uint32_t *ptr = (uint32_t*)buf;
    
        for(int j = 0; j < FAT32_ENTRIES_PER_SECTOR; j++) {
            if(i == 0 && j < 2) continue;
            if((ptr[j] & FAT32_CLUSTER_MASK) == FAT32_CLUSTER_FREE ) {
                int real_sector_number = (i * FAT32_ENTRIES_PER_SECTOR) + j;
                ptr[j] = FAT32_CLUSTER_EOC;

                if(ata_write_sector(f32_fs.fat_start + i, buf) == -1 ||
                   ata_write_sector(f32_fs.fat_start + f32_fs.sectors_per_fat + i, buf) == -1) {
                    return 0;
                }
                return real_sector_number;
            }
        }
    }
    return 0;
}

int fat32_write_cluster(uint32_t cluster, const uint8_t *buf) {

    uint32_t LBA = f32_fs.data_start + (cluster - 2) * f32_fs.sectors_per_cluster;

    for(int i = 0; i < f32_fs.sectors_per_cluster; i++) {
        if(ata_write_sector(LBA + i, buf + i * FAT32_SECTOR_SIZE) == -1) return -1;
    }
    
    return 0;

}

// Sets a specific FAT entry to a given value
// Used for chaining clusters together
// Returns 0 on success, -1 on failure
int fat32_set_cluster(uint32_t cluster, uint32_t value) {
    char buf[FAT32_SECTOR_SIZE];
    uint32_t fat_sector = f32_fs.fat_start + (cluster * FAT32_FAT_ENTRY_SIZE) / f32_fs.bytes_per_sector;
    uint32_t fat_offset = (cluster * FAT32_FAT_ENTRY_SIZE) % f32_fs.bytes_per_sector;
 
    if(ata_read_sector(fat_sector, buf) == -1) return -1;

    // 4. Cast buffer to uint32_t*
    uint32_t *ptr = (uint32_t *)buf;
    ptr[fat_offset / FAT32_FAT_ENTRY_SIZE] = value;

    if(ata_write_sector(fat_sector, buf) == -1 ||
        ata_write_sector(fat_sector + f32_fs.sectors_per_fat, buf) == -1) {
        return -1;
    }
    
    return 0;
}

// Writes data to disk, allocating clusters as needed
// Returns first cluster of the chain on success, 0 on failure
uint32_t fat32_write_file(const uint8_t *buf, uint32_t size) {
    uint32_t clusters_needed = (size + FAT32_SECTOR_MASK) / FAT32_SECTOR_SIZE;
    uint32_t first_cluster = 0, prev_cluster = 0;

    for(uint32_t i = 0; i < clusters_needed; i++) {
        int new_cluster = fat32_alloc_cluster();
        if(new_cluster == -1) return 0;

        if(prev_cluster != 0) {
            if(fat32_set_cluster(prev_cluster,new_cluster) == -1) return 0;
        }
        
        if(first_cluster == 0) first_cluster = new_cluster;
        fat32_write_cluster(new_cluster, buf + (i * FAT32_SECTOR_SIZE));

        prev_cluster = new_cluster;
    }

    return first_cluster;
}


int fat32_format_83(const char *filename, uint8_t *dst) {
    memset(dst, FAT32_83_PAD, 11);

    int index_of_dot = -1;
    for(uint8_t i = 0; i < strlen(filename); i++) {
        if(filename[i] == '.') {
            index_of_dot = i; 
            break;
        }
    }

    if(index_of_dot == -1) return -1;
    
    for(uint8_t i = 0; i < index_of_dot; i++) {
        if(i == 8) break;
        if (filename[i] >= 'a' && filename[i] <= 'z') {
            dst[i] = filename[i] - ASCII_CASE_DIFF;
            continue;
        }
        dst[i] = filename[i];
    }

    for(uint8_t i = 0; i < 3 && filename[index_of_dot + 1 + i] != '\0'; i++) {
        if (filename[index_of_dot + 1 + i] >= 'a' && filename[index_of_dot + 1 + i] <= 'z') {
            dst[8 + i] = filename[index_of_dot + 1 + i] - ASCII_CASE_DIFF;
            continue;
        }
        dst[8 + i] = filename[index_of_dot + 1 + i];
    }

    return 0;
}

int fat32_create_dirent(uint32_t dir_cluster, const char *filename,
                        uint32_t first_cluster, uint32_t size) {
    uint8_t buf[FAT32_SECTOR_SIZE];

    if(fat32_read_cluster(dir_cluster, buf) == -1) return -1;
    
    fat32_dirent_t *entry = (fat32_dirent_t *)buf;
    
    for(uint8_t i = 0; i < 16; i++) {
        if(entry[i].name[0] == FAT32_DIRENT_FREE || entry[i].name[0] == FAT32_DIRENT_DELETED) {
            if(fat32_format_83(filename, entry[i].name) == -1) return -1;
            entry[i].attributes = FAT32_ATTR_ARCHIVE; //regular file
            memset(entry[i].reserved, 0, sizeof(entry[i].reserved));
            entry[i].cluster_low = first_cluster & 0xFFFF;
            entry[i].cluster_high = (first_cluster >> 16) & 0xFFFF;
            entry[i].size = size;
            entry[i].time = 0;
            entry[i].date = 0;

            if(fat32_write_cluster(dir_cluster, buf) == -1) return -1;
            return 0;
        }
    }

    return -1;
}