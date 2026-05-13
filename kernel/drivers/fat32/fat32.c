#include "fat32.h"
#include "ata.h"
#include "klog.h"
#include "kstring.h"

fat32_fs_t f32_fs;

int fat32_init(uint32_t partition_lba) {
    char buf[512];
    ata_read_sector(partition_lba, buf);
    fat32_bpb_t *bpb = (fat32_bpb_t *)buf;
    
    if(bpb->bytes_per_sector != 512) {
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
    uint32_t fat_sector = f32_fs.fat_start + (cluster * 4) / f32_fs.bytes_per_sector;
    uint32_t fat_offset = (cluster * 4) % f32_fs.bytes_per_sector;
    char buf[512];
    
    ata_read_sector(fat_sector, buf);
    uint32_t *ptr = (uint32_t*) buf; 
    uint32_t entry = ptr[fat_offset]; //4]; //This is a coincidence that it works

    return (entry & 0x0FFFFFFF);
}

int fat32_read_cluster(uint32_t cluster, uint8_t *buf) {
    uint32_t cluster_sector = f32_fs.data_start + (cluster - 2) * f32_fs.sectors_per_cluster;

    for(uint8_t i = 0; i < f32_fs.sectors_per_cluster; i++) {
        if(ata_read_sector((cluster_sector + i), (buf + i * 512)) == -1) {
            DEBUG("[FAT32]: reading clusters went wrong. Aborting \n");
            return -1;
        }
    }
    return 0;
}

void fat32_list_dir(uint32_t cluster) {
    uint8_t buf[512];

    fat32_read_cluster(cluster, buf);

    fat32_dir_entry_t *dir_entry = (fat32_dir_entry_t *)buf;

    for(int i = 0; i < (f32_fs.sectors_per_cluster * 512) / 32; i++) {
        if(dir_entry[i].name[0] == 0x00) break;
        if(dir_entry[i].name[0] == 0xE5) continue;
        if ((dir_entry[i].attributes & 0x0F) == 0x0F) continue;
        DEBUG("[FAT32]: name: %.8s.%.3s\n", dir_entry[i].name, dir_entry[i].ext);
        DEBUG("[FAT32]: dir size: %d\n", dir_entry[i].size);
    }

    DEBUG("[FAT32]: searching for more clusters\n");
    uint32_t next_cluster = fat32_next_cluster(cluster);
    if(next_cluster >= 0x0FFFFFF8) return;

    DEBUG("[FAT32]: More clusters found at %d\n", next_cluster);
    fat32_list_dir(next_cluster);
}

int fat32_read_file(uint32_t start_cluster, uint32_t size, uint8_t *buf) {

    uint32_t bytes_read = 0;
    uint32_t cluster_size = f32_fs.sectors_per_cluster * 512;
    uint32_t current_cluster = start_cluster;
    while(1) {
        fat32_read_cluster(current_cluster, buf + bytes_read);
        bytes_read += cluster_size;
        
        if(bytes_read >= size) break;
        uint32_t next = fat32_next_cluster(current_cluster);
        if(next >= 0x0FFFFFF8) break;
        current_cluster = next;
    }

    return 0;
}

int fat32_find_file(uint32_t dir_cluster, const char *name, const char *ext, uint32_t *out_cluster, uint32_t *out_size) {
    char buf[512];
    uint32_t current = dir_cluster;
    uint32_t cluster_size = f32_fs.sectors_per_cluster * 512;

    while(1) {
        fat32_read_cluster(current, buf);
        fat32_dir_entry_t* dir_entry = (fat32_dir_entry_t*)buf;

        for(int i = 0; i < (cluster_size / 32); i++) {
            if(dir_entry[i].name[0] == 0x00) break;
            if(dir_entry[i].name[0] == 0xE5) continue;
            if ((dir_entry[i].attributes & 0x0F) == 0x0F) continue;
            if(memcmp(dir_entry[i].name, name, 8) == 0 && memcmp(dir_entry[i].ext,  ext, 3) == 0) {
                *out_cluster = (dir_entry[i].cluster_high << 16) | dir_entry[i].cluster_low;
                *out_size = dir_entry[i].size;
                return 0;
            }
        }
        uint32_t next_cluster = fat32_next_cluster(current);
        if(next_cluster >= 0x0FFFFFF8) return -1;
        current = next_cluster;
    }
    
    return -1;
}

uint32_t fat32_alloc_cluster(void) {
    uint8_t buf[512];

    for(int i = 0; i < f32_fs.sectors_per_fat; i++) {
        if(ata_read_sector((f32_fs.fat_start + i), buf) == -1) return 0;    
        uint32_t *ptr = (uint32_t*)buf;
    
        for(int j = 0; j < 128; j++) {
            if(i == 0 && j < 2) continue;
            if((ptr[j] & 0x0FFFFFFF) == 0x00000000 ) {
                int real_sector_number = (i * 128) + j;
                ptr[j] = 0x0FFFFFFF;

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
        if(ata_write_sector(LBA + i, buf + i * 512) == -1) return -1;
    }
    
    return 0;

}