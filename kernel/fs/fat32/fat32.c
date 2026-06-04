#include "fat32.h"
#include "ata.h"
#include "klog.h"
#include "kstring.h"
#include "config.h"
#include "kmalloc.h"

/*
* this could be divided in to own crud files for easy of read.
*/


fat32_fs_t f32_fs;

/////////////////////////////////////////////////////////////////////////////////////////////
/*
*   Inside calc functions.
*/

static uint32_t __fat32_calculate_lba(uint32_t cluster) {
    DEBUG("[FAT32][CALCULATE_LBA]: calculating cluster number for %d\n", cluster);
    if (cluster >= f32_fs.maximum_cluster_size) {
        ERROR("[FAT32][CALCULATE_LBA]: Cluster number too high: %d\n", cluster);
        return INVALID_LBA;
    }
    return f32_fs.fat_start + (cluster * FAT32_FAT_ENTRY_SIZE) / f32_fs.bytes_per_sector;
}

static uint32_t __fat32_cluster_to_sector(uint32_t cluster) {
    if(cluster < 2) {
        ERROR("[FAT32][CLUSTER_TO_SECTOR]: Invalid cluster number: %d\n", cluster);
        return INVALID_LBA;
    }
    return f32_fs.data_start + (cluster - 2) * f32_fs.sectors_per_cluster;
}

static uint32_t __fat32_calculate_offset(uint32_t cluster) {
    if (cluster >= f32_fs.maximum_cluster_size) {
        ERROR("[FAT32][CALCULATE_OFFSET]: Cluster number too high: %d\n", cluster);
        return INVALID_LBA;
    }
    return (cluster * FAT32_FAT_ENTRY_SIZE) % f32_fs.bytes_per_sector;
}

uint32_t fat32_calculate_cluster_size() {
    return f32_fs.sectors_per_cluster * FAT32_SECTOR_SIZE;
}

//////////////////////////////////////////////////////////////////////////////////

/*
* inside helpers
*/

static uint32_t __fat32_next_cluster(uint32_t cluster) {
    uint8_t buf[FAT32_SECTOR_SIZE];
    uint32_t lba = __fat32_calculate_lba(cluster);
    uint32_t fat_offset = __fat32_calculate_offset(cluster);

    DEBUG("[FAT32][NEXT_CLUSTER]: finding next cluster with lba: %d\n", lba);
    DEBUG("[FAT32][NEXT_CLUSTER]: offset: %d\n", fat_offset);

    
    if(ata_read_sector(lba, buf) == -1) {
        ERROR("[FAT32][NEXT_CLUSTER]: could not read sector with lba: %d\n", lba);
        return INVALID_CLUSTER;
    }
    const uint32_t *buf_ptr = (uint32_t*) buf; 
    uint32_t entry = buf_ptr[fat_offset / sizeof(uint32_t)];

    return (entry & FAT32_CLUSTER_MASK);
}

static int __fat32_read_cluster(uint32_t cluster, uint8_t *buf) {
    uint32_t cluster_sector = __fat32_cluster_to_sector(cluster);
    for(uint8_t i = 0; i < f32_fs.sectors_per_cluster; i++) {
        if(ata_read_sector((cluster_sector + i), (buf + i * FAT32_SECTOR_SIZE)) == -1) {
            ERROR("[FAT32][READ_CLUSTER]: reading clusters went wrong. Aborting \n");
            return STATUS_ERROR;
        }
    }
    return STATUS_OK;
}


static uint32_t __fat32_alloc_cluster(void) {
    uint8_t buf[FAT32_SECTOR_SIZE];

    uint32_t starting_sector_index    = f32_fs.last_allocated_cluster / FAT32_ENTRIES_PER_SECTOR; // Since we saved last allocated cluster, we can calculate the sector from which to continue
    uint32_t proposed_starting_offset = f32_fs.last_allocated_cluster % FAT32_ENTRIES_PER_SECTOR; // Calculate offset for the sector

    for(uint32_t i = starting_sector_index; i < f32_fs.sectors_per_fat; i++) {
        if(ata_read_sector((f32_fs.fat_start + i), buf) == -1) {
            ERROR("[FAT32][ALLOC_CLUSTER]: Could read sector. Stopping cluster allocation\n");
            return INVALID_CLUSTER;    
        }
        uint32_t *ptr = (uint32_t*)buf;
        
        uint32_t real_offset_start = (i == starting_sector_index) ? proposed_starting_offset : 0;

        for(uint32_t j = real_offset_start; j < FAT32_ENTRIES_PER_SECTOR; j++) {
            if(i == 0 && j < 2) continue;
            if((ptr[j] & FAT32_CLUSTER_MASK) == FAT32_CLUSTER_FREE ) {
                uint32_t cluster_index = (i * FAT32_ENTRIES_PER_SECTOR) + j;
                
                DEBUG("[FAT32][ALLOC_CLUSTER]: Free cluster found. Cluster index: %d\n", cluster_index);
                ptr[j] = FAT32_CLUSTER_EOC;

                if(ata_write_sector(f32_fs.fat_start + i, buf) == -1 ||
                   ata_write_sector(f32_fs.fat_start + f32_fs.sectors_per_fat + i, buf) == -1) {
                    ERROR("[FAT32][ALLOC_CLUSTER]: Could not write to sector. Stopping cluster allocation\n");
                    ptr[j] = FAT32_CLUSTER_FREE;
                    return INVALID_CLUSTER;
                }

                f32_fs.last_allocated_cluster = cluster_index; //save index of last allocated cluster so that next allocation can continue from it.

                return cluster_index;
            }
        }
    }
    ERROR("[FAT32][ALLOC_CLUSTER]: Allocation failed. Could not find free spot\n");
    return INVALID_CLUSTER;
}

static int __fat32_write_cluster(uint32_t cluster, const uint8_t *buf) {
    uint32_t lba = __fat32_cluster_to_sector(cluster);
    for(int i = 0; i < f32_fs.sectors_per_cluster; i++) {
        if(ata_write_sector(lba + i, buf + i * FAT32_SECTOR_SIZE) == -1) {
            ERROR("[FAT32][WRITE_CLUSTER]: Could not write to sector.\n");
            return STATUS_ERROR;
        }
    } 
    return STATUS_OK;
}

static int __fat32_set_cluster(uint32_t cluster, uint32_t value) {
    uint8_t buf[FAT32_SECTOR_SIZE];
    uint32_t lba = __fat32_calculate_lba(cluster);
    uint32_t fat_offset = __fat32_calculate_offset(cluster);
 
    if(ata_read_sector(lba, buf) == -1) {
        ERROR("[FAT32][SET_CLUSTER]: Could not read sector with lba: %d\n", lba);
        return STATUS_ERROR;
    }

    uint32_t *buf_ptr = (uint32_t *)buf;
    buf_ptr[fat_offset / sizeof(uint32_t)] = value;

    if(ata_write_sector(lba, buf) == -1 ||
        ata_write_sector(lba + f32_fs.sectors_per_fat, buf) == -1) {
        return STATUS_ERROR;
    }
    
    return STATUS_OK;
}

static uint32_t __fat32_unalloc_cluster(uint32_t cluster) {
    if(__fat32_set_cluster(cluster, FAT32_CLUSTER_FREE) == STATUS_ERROR) {
        ERROR("[FAT32][UNALLOC_CLUSTER]: Invalid cluster. Could not unallocate");
        return INVALID_CLUSTER;
    }
    return STATUS_OK;
}

static int __fat32_format_83(const char *filename, uint8_t *dst) {

    if(filename == NULL) {
        return STATUS_ERROR;
    }

    memset(dst, FAT32_83_PAD, 11);

    int index_of_dot = -1;
    for(uint8_t i = 0; i < strlen(filename); i++) {
        if(filename[i] == '.') {
            index_of_dot = i; 
            break;
        }
    }

    if(index_of_dot == -1) {
        for(uint8_t i = 0; i < strlen(filename); i++) {
            if(i == 8) break;
            if (filename[i] >= 'a' && filename[i] <= 'z') {
                dst[i] = filename[i] - ASCII_CASE_DIFF;
            } else {
                dst[i] = filename[i];
            }
        }
        return STATUS_OK;
    }
    
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

    return STATUS_OK;
}

static int __fat32_search_dir(uint32_t dir_cluster, const uint8_t *name83, uint32_t *out_cluster, uint32_t *out_size, uint8_t  *out_attr) {
    uint32_t cluster_size = fat32_calculate_cluster_size();
    uint8_t *buf = (uint8_t *)kmalloc(cluster_size);

    if(!buf) {
        return STATUS_ERROR;
    }

    uint8_t is_done = 0;
    uint32_t current = dir_cluster;

    while(is_done != 1) {
        if(__fat32_read_cluster(current, buf) == STATUS_ERROR) {
            ERROR("[FAT32][FIND_FILE]: Could not read cluster\n");
            kfree(buf);
            return STATUS_ERROR;
        }
        fat32_dirent_t* dir_entry = (fat32_dirent_t*)buf;
         uint32_t dirent_size = (f32_fs.sectors_per_cluster * FAT32_SECTOR_SIZE) / FAT32_DIRENT_SIZE;

        for(uint32_t i = 0; i < dirent_size; i++) {
            if(dir_entry[i].name[0] == FAT32_DIRENT_FREE) {
                is_done = 1;
                break;
            }

            if(dir_entry[i].name[0] == FAT32_DIRENT_DELETED) continue;
            if ((dir_entry[i].attributes & 0x0F) == 0x0F) continue;
            if(memcmp(dir_entry[i].name, name83, 11) == 0) {
                *out_cluster = (dir_entry[i].cluster_high << 16) | dir_entry[i].cluster_low;
                *out_size = dir_entry[i].size;
                *out_attr = dir_entry[i].attributes;
                DEBUG("[FAT32][FIND_FILE]: file found!\n");
                DEBUG("[FAT32][FIND_FILE]: cluster: %d\n", *out_cluster);
                DEBUG("[FAT32][FIND_FILE]: size: %d\n", *out_size);
                kfree(buf);
                return STATUS_OK;
            }
        }

        if(is_done == 1) {
            break;
        }

        uint32_t next_cluster = __fat32_next_cluster(current);
        DEBUG("[FAT32][FIND_FILE]: Next cluster number: %d\n", next_cluster);

        if(next_cluster >= FAT32_CLUSTER_EOC_MIN) {
            DEBUG("[FAT32][FIND_FILE]: End of the cluster chain\n");
            kfree(buf);
            return STATUS_ERROR;
        }
        current = next_cluster;
    }

    kfree(buf);
    return STATUS_ERROR;
}

/*
*  
*
*/


void fat32_list_dir(uint32_t cluster) {
    uint32_t cluster_size = fat32_calculate_cluster_size();
    uint8_t *buf = (uint8_t *)kmalloc(cluster_size);
    //uint8_t buf[FAT32_SECTOR_SIZE];

    if(!buf) return;

    if(__fat32_read_cluster(cluster, buf) == STATUS_ERROR) {
        ERROR("[FAT32][LIST_DIR]: Could not read cluster. Aborting\n");
        kfree(buf);
        return;
    }

    const fat32_dirent_t *dir_entry = (fat32_dirent_t *)buf;

    for(int i = 0; i < (f32_fs.sectors_per_cluster * FAT32_SECTOR_SIZE) / FAT32_DIRENT_SIZE; i++) {
        if(dir_entry[i].name[0] == FAT32_DIRENT_FREE) break;
        if(dir_entry[i].name[0] == FAT32_DIRENT_DELETED) continue;
        if ((dir_entry[i].attributes & 0x0F) == 0x0F) continue;
        DEBUG("[FAT32][LIST_DIR]: name: %.11s\n", dir_entry[i].name);
        DEBUG("[FAT32][LIST_DIR]: dir size: %d\n", dir_entry[i].size);
    }


    DEBUG("[FAT32][LIST_DIR]: searching for more clusters\n");
    uint32_t next_cluster = __fat32_next_cluster(cluster);

    if(next_cluster >= FAT32_CLUSTER_EOC_MIN) {
        DEBUG("[FAT32][LIST_DIR]: End of the chain reached\n");
        kfree(buf);
        return;
    }

    DEBUG("[FAT32][LIST_DIR]: More clusters found at %d\n", next_cluster);
    kfree(buf);
    fat32_list_dir(next_cluster);
} //list_dir


int fat32_read_file(uint32_t start_cluster, uint32_t size, uint8_t *buf) {

    uint32_t bytes_read = 0;
    uint32_t cluster_size = fat32_calculate_cluster_size();
    uint32_t current_cluster = start_cluster;

    //uint8_t *temp_buf = (uint8_t *)kmalloc(cluster_size);

    while(1) {
        if(__fat32_read_cluster(current_cluster, buf + bytes_read) == STATUS_ERROR) {
            ERROR("[FAT32][READ_FILE]: Error reading cluster\n");
      //    kfree(temp_buf);
            return STATUS_ERROR;
        }
        
        uint32_t bytes_left = size - bytes_read;
        uint32_t bytes_to_copy = (bytes_left > cluster_size) ? cluster_size : bytes_left;
        bytes_read += bytes_to_copy;

        if(bytes_read >= size) {
            DEBUG("[FAT32][READ_FILE]: All requested bytes read successfully.\n");
            DEBUG("[FAT32][READ_FILE]: Bytes read: %d\n", bytes_read);
            DEBUG("[FAT32][READ_FILE]: Bytes copied: %d\n", bytes_to_copy);
            return STATUS_OK;
        }

        uint32_t next = __fat32_next_cluster(current_cluster);
        
        if(next >= FAT32_CLUSTER_EOC_MIN) {
            return STATUS_ERROR;
        }
        current_cluster = next;
    }

    return STATUS_OK;
} //read_file

int fat32_find_file(const char *path, uint32_t *out_cluster, uint32_t *out_size) {
    
    if(path == NULL) {
        ERROR("[FAT32][FIND_FILE]: Given path was invalid\n");
        return STATUS_ERROR;
    }

    uint32_t current_cluster = f32_fs.root_cluster;
    const char *p = path;

    if(p[0] == '/') p++;

    while (*p) {
        char segment[13];
        uint8_t index = 0;
        while(index < 12) {
            if(p[index] == '\0' || p[index] == '/') {
                segment[index] = '\0';
                break;
            }

            segment[index] = p[index];
            index++;
        }

        p += index;  

        uint8_t name83[11];
            if(__fat32_format_83(segment, name83) == STATUS_ERROR) {
                ERROR("[FAT32][FIND_FILE]: was Unable to format the filename\n");
                return STATUS_ERROR;
            }

        uint32_t found_cluster, found_size;
        uint8_t  found_attr;
        if(__fat32_search_dir(current_cluster, name83, &found_cluster, &found_size, &found_attr) == STATUS_ERROR) {
            ERROR("[FAT32][FIND_FILE]: Unable to find the directory\n");
            return STATUS_ERROR;
        }

        if (*p == '\0') {
            DEBUG("[FAT32][FIND_FILE]: Directory cluster found: %d\n", found_cluster);
            DEBUG("[FAT32][FIND_FILE]: Size: %d\n", found_size);
            *out_cluster = found_cluster;
            *out_size    = found_size;
            return STATUS_OK;
        } else {


            if(p[0] == '/') p++;
            //See if directory bit is present
            if(!(found_attr & FAT32_ATTR_DIRECTORY)) { 
                ERROR("[FAT32][FIND_FILE]: Found invalid attributes\n");
                return STATUS_ERROR;
            }
            current_cluster = found_cluster;
        }
    }

    return STATUS_ERROR;
}

uint32_t fat32_write_file(const uint8_t *buf, uint32_t size) {
    uint32_t cluster_size = fat32_calculate_cluster_size();
    uint32_t clusters_needed = (size + (cluster_size - 1)) / cluster_size;
    DEBUG("[FAT32][WRITE_FILE]: clusters needed for the file %d\n", clusters_needed);
    uint32_t first_cluster = 0, prev_cluster = 0;

    for(uint32_t i = 0; i < clusters_needed; i++) {
        uint32_t new_cluster = __fat32_alloc_cluster();
        if(new_cluster == INVALID_CLUSTER) {
            ERROR("[FAT32][WRITE_FILE]: Did not receive a cluster. Aborting\n");
            return INVALID_CLUSTER;
        }

        if(prev_cluster != 0) {
            if(__fat32_set_cluster(prev_cluster,new_cluster) == STATUS_ERROR) {
                ERROR("[FAT32][WRITE_FILE]: Was not able to chain clusters. Aborting\n");
                return INVALID_CLUSTER;
            }
        }
        
        if(first_cluster == 0) first_cluster = new_cluster;
        if(__fat32_write_cluster(new_cluster, buf + (i * cluster_size)) == STATUS_ERROR) {
            __fat32_unalloc_cluster(new_cluster);
            ERROR("[FAT32][WRITE_FILE]: Could not write to cluster. Aborting\n");
            return INVALID_CLUSTER;
        }

        prev_cluster = new_cluster;
    }

    return first_cluster;
} //write_file


int fat32_create_dirent(uint32_t dir_cluster, const char *filename, uint32_t first_cluster, uint32_t size) {
    uint32_t cluster_size = fat32_calculate_cluster_size();
    uint8_t *buf = (uint8_t *)kmalloc(cluster_size);
    //uint8_t buf[FAT32_SECTOR_SIZE];
    if(!buf) return STATUS_ERROR;

    if(__fat32_read_cluster(dir_cluster, buf) == STATUS_ERROR) {
        ERROR("[FAT32][CREATE_DIRENT]: Could not read cluster\n");
        kfree(buf);
        return STATUS_ERROR;
    }
    
    fat32_dirent_t *entry = (fat32_dirent_t *)buf;
    uint32_t cluster_length = (f32_fs.sectors_per_cluster * FAT32_SECTOR_SIZE) / FAT32_DIRENT_SIZE; 

    for(uint8_t i = 0; i < cluster_length; i++) {
        if(entry[i].name[0] == FAT32_DIRENT_FREE || entry[i].name[0] == FAT32_DIRENT_DELETED) {
            if(__fat32_format_83(filename, entry[i].name) == STATUS_ERROR) {
                ERROR("[FAT32][CREATE_DIRENT]: Could not format the name\n");
                return STATUS_ERROR;
            }
            entry[i].attributes = FAT32_ATTR_ARCHIVE;
            memset(entry[i].reserved, 0, sizeof(entry[i].reserved));
            entry[i].cluster_low = first_cluster & 0xFFFF;
            entry[i].cluster_high = (first_cluster >> 16) & 0xFFFF;
            entry[i].size = size;
            entry[i].time = 0;
            entry[i].date = 0;

            if(__fat32_write_cluster(dir_cluster, buf) == STATUS_ERROR) return STATUS_ERROR;
            kfree(buf);
            return STATUS_OK;
        }
    }

    kfree(buf);
    return STATUS_ERROR;
} //create_dirent


int fat32_init(uint32_t partition_lba) {
    uint8_t buf[FAT32_SECTOR_SIZE];
    ata_read_sector(partition_lba, buf);
    const fat32_bpb_t *bpb = (fat32_bpb_t *)buf;
    
    if(bpb->bytes_per_sector != FAT32_SECTOR_SIZE) {
        DEBUG("[FAT32][INIT]: bytes_pre_sector lower than 512. Is: %d\n", bpb->bytes_per_sector);
    }

    f32_fs.partition_lba          = partition_lba;
    f32_fs.fat_start              = partition_lba + bpb->reserved_sectors;
    f32_fs.data_start             = f32_fs.fat_start + (bpb->fat_count * bpb->sectors_per_fat);
    f32_fs.root_cluster           = bpb->root_cluster;
    f32_fs.sectors_per_cluster    = bpb->sectors_per_cluster;
    f32_fs.bytes_per_sector       = bpb->bytes_per_sector;
    f32_fs.fat_count              = bpb->fat_count;
    f32_fs.sectors_per_fat        = bpb->sectors_per_fat;
    f32_fs.last_allocated_cluster = 0;
    uint32_t data_sectors  = bpb->total_sectors_32 - bpb->reserved_sectors - (bpb->fat_count * bpb->sectors_per_fat);
    f32_fs.maximum_cluster_size = (data_sectors / bpb->sectors_per_cluster) + 2; // 2 for reserved clusters

    DEBUG("[FAT32][INIT]: partition_lba: %d\n", f32_fs.partition_lba);
    DEBUG("[FAT32][INIT]: fat_start: %d\n", f32_fs.fat_start);
    DEBUG("[FAT32][INIT]: data_start: %d\n", f32_fs.data_start);
    DEBUG("[FAT32][INIT]: root_cluster: %d\n", f32_fs.root_cluster);
    DEBUG("[FAT32][INIT]: sectors_per_cluster: %d\n", f32_fs.sectors_per_cluster);
    DEBUG("[FAT32][INIT]: bytes_per_sector: %d\n", f32_fs.bytes_per_sector);
    DEBUG("[FAT32][INIT]: fat_count: %d\n", f32_fs.fat_count);
    DEBUG("[FAT32][INIT]: sectors_per_fat %d\n", f32_fs.sectors_per_fat);
    DEBUG("[FAT32][INIT]: maximum cluster size: %d\n", f32_fs.maximum_cluster_size);
    
    return STATUS_OK;
} //init