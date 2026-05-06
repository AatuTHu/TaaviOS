#include "fat32.h"
#include "ata.h"
#include "klog.h"

fat32_fs_t fat32_fs;

int fat32_init(uint32_t partition_lba) {
    char buf[512];
    ata_read_sector(partition_lba, buf);
    fat32_bpb_t *bpb = (fat32_bpb_t *)buf;
    
    if(bpb->bytes_per_sector != 512) {
        DEBUG("[FAT32]: bytes_pre_sector lower than 512. Is: %d\n", bpb->bytes_per_sector);
    }

    fat32_fs.partition_lba          = partition_lba;
    fat32_fs.fat_start              = partition_lba + bpb->reserved_sectors;
    fat32_fs.data_start             = fat32_fs.fat_start + (bpb->fat_count * bpb->sectors_per_fat);
    fat32_fs.root_cluster           = bpb->root_cluster;
    fat32_fs.sectors_per_cluster    = bpb->sectors_per_cluster;
    fat32_fs.bytes_per_sector       = bpb->bytes_per_sector;
    fat32_fs.fat_count              = bpb->fat_count;
    fat32_fs.sectors_per_fat        = bpb->sectors_per_fat;

    DEBUG("[FAT32]: partition_lba: %d\n", fat32_fs.partition_lba);
    DEBUG("[FAT32]: fat_start: %d\n", fat32_fs.fat_start);
    DEBUG("[FAT32]: data_start: %d\n", fat32_fs.data_start);
    DEBUG("[FAT32]: root_cluster: %d\n", fat32_fs.root_cluster);
    DEBUG("[FAT32]: sectors_per_cluster: %d\n", fat32_fs.sectors_per_cluster);
    DEBUG("[FAT32]: bytes_pre_sector: %d\n", fat32_fs.bytes_per_sector);
    DEBUG("[FAT32]: fat_count: %d\n", fat32_fs.fat_count);
    DEBUG("[FAT32]: sector_per_fat %d\n", fat32_fs.sectors_per_fat);
    
    return 0;
}

uint32_t fat32_next_cluster(uint32_t cluster) {
    uint32_t fat_sector = fat32_fs.fat_start + (cluster * 4) / fat32_fs.bytes_per_sector;
    uint32_t fat_offset = (cluster * 4) % fat32_fs.bytes_per_sector;
    char buf[512];
    
    ata_read_sector(fat_sector, buf);
    uint32_t *ptr = (uint32_t*) buf; 
    uint32_t entry = ptr[fat_offset/4];

    return (entry & 0x0FFFFFFF);
}

int fat32_read_cluster(uint32_t cluster, uint8_t *buf) {
    uint32_t cluster_sector = fat32_fs.data_start + (cluster - 2) * fat32_fs.sectors_per_cluster;

    for(uint8_t i = 0; i < fat32_fs.sectors_per_cluster; i++) {
        if(ata_read_sector((cluster_sector + i), (buf + i*512)) == -1) {
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

    for(int i = 0; i < (fat32_fs.sectors_per_cluster * 512) / 32; i++) {
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
    // TODO: track how many bytes read so far
    // TODO: cluster_size = sectors_per_cluster * 512
    // TODO: loop:
    //       read current cluster into buf + bytes_read
    //       bytes_read += cluster_size
    //       if bytes_read >= size, break
    //       next = fat32_next_cluster(current)
    //       if next >= 0x0FFFFFF8, break
    //       current = next
    // TODO: return 0 on success
}