#include "fat32.h"

fat32_fs_t f32_fs;

/*
* Find a file, at the takes in a path and give back cluster and size
*/
int fat32_find_file(const char *path, uint32_t *out_cluster, uint32_t *out_size) {
    
    if(path == NULL) {
        ERROR("[FAT32][FIND_FILE]: Given path was invalid\n");
        return STATUS_ERROR;
    }

    //currenct cluster starts from root_cluster as we start looking from there
    uint32_t current_directory_cluster = f32_fs.root_cluster;
    uint32_t found_cluster, found_size;
    uint8_t  found_attr;
    uint8_t name83[11];
    const char *inner_copy_of_path = path;

    //if the first index of copied path is a forward slash we skip it by advancing the pointer
    //This is because the inner loop would brake at the first iteration
    if(inner_copy_of_path[0] == '/') inner_copy_of_path++;


    while (*inner_copy_of_path) {
        char segment[13]; //one segment is name (8) dot (1) and ext (3) + '\0' (1).
        uint8_t index = 0;

        //since there shouldnt be ha '/' at this point we only go from 0 to 11 forward
        //breaking the loop if we find end of file or a slash sooner
        while(index < 12) {
            if(inner_copy_of_path[index] == '\0' || inner_copy_of_path[index] == '/') {
                segment[index] = '\0';
                break;
            }
            segment[index] = inner_copy_of_path[index];
            index++;
        }
        //quaranteen that segment ends with null terminator
        segment[index] = '\0';
        //advacne the size of index the inner copy for the next iteration of the main loop
        inner_copy_of_path += index;  

        //with the new segment we format it to 83 standard. The formatted segment is copied to name83 array in the __fat32_format_83 function
        if(__fat32_format_83(segment, name83) == STATUS_ERROR) {
            ERROR("[FAT32][FIND_FILE]: was Unable to format the filename\n");
            return STATUS_ERROR;
        }

        // Search the current directory cluster for the formatted 8.3 name
        if(__fat32_search_dir(current_directory_cluster, name83, &found_cluster, &found_size, &found_attr) == STATUS_ERROR) {
            ERROR("[FAT32][FIND_FILE]: Unable to find the directory\n");
            return STATUS_ERROR;
        }

        //if the inner_copy_of_path is now pointing to a value '\0' we have reached the end of the path and found what we were looking for.
        //however if it is a forward slash advance by one and mask the attributes to see that it is a directory and not a file
        if (*inner_copy_of_path == '\0') {
            DEBUG("[FAT32][FIND_FILE]: Directory cluster found: %d\n", found_cluster);
            DEBUG("[FAT32][FIND_FILE]: Size: %d\n", found_size);
            *out_cluster = found_cluster;
            *out_size    = found_size;
            return STATUS_OK;
        } else {
            if(inner_copy_of_path[0] == '/') inner_copy_of_path++;
            //See if directory bit is present
            if(!(found_attr & FAT32_ATTR_DIRECTORY)) { 
                ERROR("[FAT32][FIND_FILE]: Found invalid attributes\n");
                return STATUS_ERROR;
            }
            //step further in to a subdirectory
            current_directory_cluster = found_cluster;
        }
    }

    return STATUS_ERROR;
}


/*
* Update the file size property of a target file. 
* Begins searching for the file's entry starting from the directory cluster provided in dir_start_cluster.
*/
int fat32_update_dirent_size(uint32_t starting_cluster, uint32_t file_cluster, uint32_t new_size) {

    DEBUG("[FAT32][UPDATE_DIRENT_SIZE]: startibg cluster: %d\n", starting_cluster);
    DEBUG("[FAT32][UPDATE_DIRENT_SIZE]: file_cluster: %d\n", file_cluster);
    DEBUG("[FAT32][UPDATE_DIRENT_SIZE]: new_size: %d\n", new_size);


    uint8_t *buf = __fat32_allocate_buffer();
    if(buf == INVALID_BUFFER) return STATUS_ERROR;
    
    uint32_t cluster_size = fat32_calculate_cluster_size();
    uint32_t current_dir_cluster = starting_cluster;

    /*
    * Start the loop by reading data of the directory cluster
    */
    while(1) {
        if(__fat32_read_cluster(current_dir_cluster, buf) == STATUS_ERROR) {
            ERROR("[FAT32][UPDATE_DIRENT_SIZE]: could not read cluster\n");
            __fat32_free_buffer(buf);
            return STATUS_ERROR;
        }
        
        /*
        * cast the read info to be a directory entry structure
        * calculate of many entry elements there can be in a directory are in a directory
        */
        fat32_dirent_t *dir_entry = (fat32_dirent_t*)buf;
        uint32_t max_dir_entries = cluster_size / FAT32_DIRENT_SIZE;

        DEBUG("[FAT32][UPDATE_DIRENT_SIZE]: dir_enty: %s\n", dir_entry->name);
        DEBUG("[FAT32][UPDATE_DIRENT_SIZE]: max_dir_entries: %d\n", max_dir_entries);

        /*
        * loop the size of dir_enry. At every iteration construct a cluster from the high and low cluster parts
        * Check if it is the same as the file cluster in question. if it is update the current directory entry with the give size
        * Then write tha changes to hardware.
        */

        for(uint32_t i = 0; i < max_dir_entries; i++) {
            uint32_t whole_cluster = (dir_entry[i].cluster_high << 16) | dir_entry[i].cluster_low;
            DEBUG("[FAT32][UPDATE_DIRENT_SIZE]: whole cluster: %d\n", whole_cluster);
            if(whole_cluster == file_cluster) {
                DEBUG("[FAT32][UPDATE_DIRENT_SIZE]: Right dir cluster found!\n");
                dir_entry[i].size = new_size;
                if(__fat32_write_cluster(current_dir_cluster, buf) == STATUS_ERROR) {
                    __fat32_free_buffer(buf);
                    return STATUS_ERROR;
                }
                DEBUG("[FAT32][UPDATE_DIRENT_SIZE]: Update complete!\n");
                __fat32_free_buffer(buf);
                return STATUS_OK;
            }
        }

        /*
        * The target file entry was not in this directory cluster block. 
        * Follow the chain link to fetch the next directory cluster block from the FAT.
        */
        uint32_t next = __fat32_next_cluster(current_dir_cluster);

        if(next >= FAT32_CLUSTER_EOC) {
            ERROR("[FAT32][UPDATE_DIRENT_SIZE]: Next cluster is end of the chain. Aborting\n");
            __fat32_free_buffer(buf);
            return STATUS_ERROR;
        }

        current_dir_cluster = next;
    }

    __fat32_free_buffer(buf);
    return STATUS_ERROR;
}

/*
* This function creates a new directory entry (metadata slot) 
* inside a parent directory cluster using the given parameters.
*/
int fat32_create_dirent(uint32_t starting_cluster, const char *filename, uint32_t first_cluster, uint32_t size) {
    
    uint8_t *buf = __fat32_allocate_buffer();
    if(buf == INVALID_BUFFER) return STATUS_ERROR;

    /*
    * first read the data off the given starting cluster to buffer 
    */

    if(__fat32_read_cluster(starting_cluster, buf) == STATUS_ERROR) {
        ERROR("[FAT32][CREATE_DIRENT]: Could not read cluster\n");
        __fat32_free_buffer(buf);
        return STATUS_ERROR;
    }
    
    fat32_dirent_t *entry = (fat32_dirent_t *)buf;
    uint32_t max_entries = __fat32_calculate_maximum_number_of_directory_entries(); 

    /*
    * loop from 0 to max entries
    * check if the current entry name equals to free or deleted. if it does
    * give the given filename and the current entry name to __fat32_format_83 function. The function will format and save the filename to entrys name
    * Then deposit some starting info to the entry. Deconstruct the first cluster of the directory into a high and low clusters.
    * write the new entry to disk
    */
    for(uint32_t i = 0; i < max_entries; i++) {
        if(entry[i].name[0] == FAT32_DIRENT_FREE || entry[i].name[0] == FAT32_DIRENT_DELETED) {
            if(__fat32_format_83(filename, entry[i].name) == STATUS_ERROR) {
                ERROR("[FAT32][CREATE_DIRENT]: Could not format the name\n");
                break;
            }
            entry[i].attributes = FAT32_ATTR_ARCHIVE;
            memset(entry[i].reserved, 0, sizeof(entry[i].reserved));
            entry[i].cluster_low = first_cluster & 0xFFFF;
            entry[i].cluster_high = (first_cluster >> 16) & 0xFFFF;
            entry[i].size = size;
            entry[i].time = 0;
            entry[i].date = 0;

            if(__fat32_write_cluster(starting_cluster, buf) == STATUS_ERROR) break;
            __fat32_free_buffer(buf);
            return STATUS_OK;
        }
    }

    __fat32_free_buffer(buf);
    return STATUS_ERROR;
} //create_dirent

// constucts a fat table of from the partition lba
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