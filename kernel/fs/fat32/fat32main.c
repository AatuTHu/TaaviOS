#include "fat32.h"

fat32_fs_t f32_fs;

/**
 * __fat32_find_file() - Finds the specific file.
 *
 * @path:        holds the route of the file
 * @out_cluster: after the function this should hold the root cluster of the
 * file
 * @out_size:    after the function this should hold the size of the file
 *
 * Description:
 * Searches for a file using the given path. Deconstructs the path into segments
 * and gontinues to next directory by using those segments if the file was not
 * found sooner.
 *
 * Return: STATUS_ERROR || STATUS_OK.
 */
int fat32_find_file(const char *path, uint32_t *out_cluster, uint32_t *out_size,
                    char *out_fname) {

    if (path == NULL) {
        ERROR("[FAT32][FIND_FILE]: Given path was invalid\n");
        return STATUS_ERROR;
    }

    // currenct cluster starts from root_cluster as we start looking from there
    uint32_t current_directory_cluster = f32_fs.root_cluster;
    uint32_t found_cluster, found_size;
    uint8_t found_attr;
    uint8_t name83[11];

    // if the first index of copied path is a forward slash we skip it by
    // advancing the pointer This is because the inner loop would brake at the
    // first iteration
    if (path[0] == '/')
        path++;

    while (*path) {
        char segment[13]; // one segment is name (8) dot (1) and ext (3) + '\0'
                          // (1).
        uint8_t index = 0;

        // since there shouldnt be ha '/' at this point we only go from 0 to 11
        // forward breaking the loop if we find end of file or a slash sooner
        while (index < 12) {
            if (path[index] == '\0' || path[index] == '/') {
                segment[index] = '\0';
                break;
            }
            segment[index] = path[index];
            index++;
        }
        // quaranteen that segment ends with null terminator
        segment[index] = '\0';
        // advacne the size of index the inner copy for the next iteration of
        // the main loop
        path += index;

        // with the new segment we format it to 83 standard. The formatted
        // segment is copied to name83 array in the __fat32_format_83 function
        if (__fat32_format_83(segment, name83) == STATUS_ERROR) {
            ERROR("[FAT32][FIND_FILE]: was Unable to format the filename\n");
            return STATUS_ERROR;
        }

        // Search the current directory cluster for the formatted 8.3 name
        if (__fat32_search_dir(current_directory_cluster, name83,
                               &found_cluster, &found_size,
                               &found_attr) == STATUS_ERROR) {
            ERROR("[FAT32][FIND_FILE]: Unable to find the directory\n");
            return STATUS_ERROR;
        }

        // if the path is now pointing to a value '\0' we have
        // reached the end of the path and found what we were looking for.
        // however if it is a forward slash advance by one and mask the
        // attributes to see that it is a directory and not a file
        if (*path == '\0') {
            memcpy(out_fname, segment, 8);
            *out_cluster = found_cluster;
            *out_size    = found_size;
            DEBUG("[FAT32][FIND_FILE]: Directory cluster found: %d\n",
                  *out_cluster);
            DEBUG("[FAT32][FIND_FILE]: Size: %d\n", *out_size);
            DEBUG("[FAT32][FIND_FILE]: last segment: %s\n", out_fname);
            return STATUS_OK;
        }
        if (path[0] == '/')
            path++;
        // See if directory bit is present
        if (!(found_attr & FAT32_ATTR_DIRECTORY)) {
            ERROR("[FAT32][FIND_FILE]: Found invalid attributes\n");
            return STATUS_ERROR;
        }
        // step further in to a subdirectory
        current_directory_cluster = found_cluster;
    }

    return STATUS_ERROR;
}

/**
* __fat32_next_cluster() - Updates directory metadata.
*
* @starting_cluster: directory cluster
* @file_cluster:     child of directory cluster
* @new_size:         -

* Description:
* After succesfull write we want to update a specific file in a specific
directory
* This function searches the right one and updates the metadata to disk.
*
* Return: STATUS_ERROR || STATUS_OK.
*/
int fat32_update_dirent_size(uint32_t starting_cluster, uint32_t file_cluster,
                             uint32_t new_size) {

    DEBUG("[FAT32][UPDATE_DIRENT_SIZE]: startibg cluster: %d\n",
          starting_cluster);
    DEBUG("[FAT32][UPDATE_DIRENT_SIZE]: file_cluster: %d\n", file_cluster);
    DEBUG("[FAT32][UPDATE_DIRENT_SIZE]: new_size: %d\n", new_size);

    uint8_t *buf = __fat32_allocate_buffer();
    if (buf == INVALID_BUFFER)
        return STATUS_ERROR;

    uint32_t cluster_size        = __fat32_calculate_cluster_size();
    uint32_t current_dir_cluster = starting_cluster;

    /*
     * Start the loop by reading data of the directory cluster
     */
    while (1) {
        if (__fat32_read_cluster(current_dir_cluster, buf) == STATUS_ERROR) {
            ERROR("[FAT32][UPDATE_DIRENT_SIZE]: could not read cluster\n");
            __fat32_free_buffer(buf);
            return STATUS_ERROR;
        }

        /*
         * cast the read info to be a directory entry structure
         * calculate of many entry elements there can be in a directory are in a
         * directory
         */
        fat32_dirent_t *dir_entry = (fat32_dirent_t *)buf;
        uint32_t max_dir_entries  = cluster_size / FAT32_DIRENT_SIZE;

        DEBUG("[FAT32][UPDATE_DIRENT_SIZE]: dir_enty: %s\n", dir_entry->name);
        DEBUG("[FAT32][UPDATE_DIRENT_SIZE]: max_dir_entries: %d\n",
              max_dir_entries);

        /*
         * loop the size of dir_enry. At every iteration construct a cluster
         * from the high and low cluster parts Check if it is the same as the
         * file cluster in question. if it is update the current directory entry
         * with the give size Then write tha changes to hardware.
         */

        for (uint32_t i = 0; i < max_dir_entries; i++) {
            uint32_t whole_cluster =
                (dir_entry[i].cluster_high << 16) | dir_entry[i].cluster_low;
            DEBUG("[FAT32][UPDATE_DIRENT_SIZE]: whole cluster: %d\n",
                  whole_cluster);
            if (whole_cluster == file_cluster) {
                DEBUG(
                    "[FAT32][UPDATE_DIRENT_SIZE]: Right dir cluster found!\n");
                dir_entry[i].size = new_size;
                if (__fat32_write_cluster(current_dir_cluster, buf) ==
                    STATUS_ERROR) {
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
         * Follow the chain link to fetch the next directory cluster block from
         * the FAT.
         */
        uint32_t next = __fat32_next_cluster(current_dir_cluster);

        if (next >= FAT32_CLUSTER_EOC) {
            ERROR("[FAT32][UPDATE_DIRENT_SIZE]: Next cluster is end of the "
                  "chain. Aborting\n");
            __fat32_free_buffer(buf);
            return STATUS_ERROR;
        }

        current_dir_cluster = next;
    }

    __fat32_free_buffer(buf);
    return STATUS_ERROR;
}

/**
 * __fat32_create_mkdirp() - Creates directory entry.
 *
 * @starting_cluster: starting directory cluster
 * @path: directory path
 *
 * Description:
 * This function makes a chain of directories as long as the path is
 * Before making a new directory it check if on with the same name exsist
 * thus preventing a dublication.
 *
 * Return: STATUS_ERROR || STATUS_OK.
 */
int fat32_mkdirp(uint32_t parent_cluster, const char *path) {

    DEBUG("[FAT32][MKDIRP]: Current parent_cluster %d\n", parent_cluster);
    DEBUG("[FAT32][MKDIRP]: Current path %s\n", path);

    uint8_t *buf = __fat32_allocate_buffer();
    if (buf == INVALID_BUFFER)
        return STATUS_ERROR;

    uint32_t current_directory_cluster = parent_cluster;
    uint32_t max_entries               = __fat32_calculate_max_dir_entries();
    uint8_t name83[11];

    while (*path) {

        if (__fat32_read_cluster(current_directory_cluster, buf) ==
            STATUS_ERROR) {
            ERROR("[FAT32][MKDIRP]: Could not read cluster\n");
            __fat32_free_buffer(buf);
            return STATUS_ERROR;
        }

        fat32_dirent_t *dir_entry = (fat32_dirent_t *)buf;

        if (__fat32_walk_dir_path(&path, name83) == STATUS_ERROR) {
            DEBUG("[FAT32][MKDIRP]: Problem in path walking");
            __fat32_free_buffer(buf);
            return STATUS_ERROR;
        }

        int found = 0;
        for (uint32_t i = 0; i < max_entries; i++) {
            if (dir_entry[i].name[0] == FAT32_DIRENT_FREE ||
                dir_entry[i].name[0] == FAT32_DIRENT_DELETED)
                continue;

            if (memcmp(name83, dir_entry[i].name, 11) == 0 &&
                dir_entry[i].attributes & FAT32_ATTR_DIRECTORY) {
                DEBUG("[FAT32][MKDIRP]: Match found %s\n", dir_entry[i].name);
                current_directory_cluster = (dir_entry[i].cluster_high << 16) |
                                            dir_entry[i].cluster_low;
                found                     = 1;
                break;
            }
        }

        if (found == 1) {
            DEBUG("[FAT32][MKDIRP]: match found!\n");
            continue;
        }

        DEBUG("[FAT32][MKDIRP]: There were no match in the directory\n");
        int slot_found = 0;
        for (uint32_t i = 0; i < max_entries; i++) {
            if (dir_entry[i].name[0] == FAT32_DIRENT_FREE ||
                dir_entry[i].name[0] == FAT32_DIRENT_DELETED) {
                uint32_t new_dir_cluster = __fat32_alloc_cluster();

                if (new_dir_cluster == INVALID_CLUSTER) {
                    ERROR("[FAT32][MKDIRP]: failed to allocate new cluster\n");
                    __fat32_free_buffer(buf);
                    return STATUS_ERROR;
                }

                DEBUG("[FAT32][MKDIRP]: Chaining next cluster to "
                      "be END OF CHAIN\n");
                if (__fat32_set_cluster(new_dir_cluster, FAT32_CLUSTER_EOC) ==
                    STATUS_ERROR) {
                    ERROR("[FAT32][MKDIRP]: failed to set cluster as EOC\n");
                    __fat32_free_buffer(buf);
                    return STATUS_ERROR;
                }

                memcpy(dir_entry[i].name, name83, 11);
                DEBUG("[FAT32][MKDIRP]: New dir name %s\n", dir_entry[i].name);
                memset(dir_entry[i].reserved, 0, sizeof(dir_entry[i].reserved));

                dir_entry[i].cluster_low  = new_dir_cluster & 0xFFFF;
                dir_entry[i].cluster_high = (new_dir_cluster >> 16) & 0xFFFF;
                dir_entry[i].attributes   = FAT32_ATTR_DIRECTORY;
                dir_entry[i].size         = 0;
                dir_entry[i].time         = 0;
                dir_entry[i].date         = 0;
                if (__fat32_write_cluster(current_directory_cluster, buf) ==
                    STATUS_ERROR) {
                    ERROR("[FAT32][MKDIRP]: Failed to write directory entry to "
                          "disk\n");
                    __fat32_free_buffer(buf);
                    return STATUS_ERROR;
                }

                memset(buf, 0, __fat32_calculate_cluster_size());
                __fat32_write_cluster(new_dir_cluster, buf);

                current_directory_cluster = new_dir_cluster;
                slot_found                = 1;
                break;
            }
        }

        if (!slot_found) {
            uint32_t next_cluster =
                __fat32_next_cluster(current_directory_cluster);
            if (next_cluster >= FAT32_CLUSTER_EOC_MIN) {
                DEBUG("[FAT32][MKDIRP]: Next cluster was end ofchain\n");
                next_cluster = __fat32_alloc_cluster();

                if (next_cluster == INVALID_CLUSTER) {
                    DEBUG("[FAT32][MKDIRP]: failed to allocate newcluster\n");
                    break;
                }

                DEBUG("[FAT32][MKDIRP]: Chaining next cluster to "
                      "current_directory_cluster\n");
                if (__fat32_set_cluster(current_directory_cluster,
                                        next_cluster) == STATUS_ERROR) {
                    ERROR("[FAT32][MKDIRP]: Failed to chainclusters\n");
                    break;
                }

                DEBUG("[FAT32][MKDIRP]: marking new cluster to be end of "
                      "chain\n");

                if (__fat32_set_cluster(next_cluster, FAT32_CLUSTER_EOC) ==
                    STATUS_ERROR) {
                    ERROR("[FAT32][MKDIRP]: Failed to set new cluster as "
                          "endofchain\n");
                    break;
                }

                current_directory_cluster = next_cluster;
            }

            current_directory_cluster = next_cluster;
        }
    }

    DEBUG("[FAT32][MKDIRP]: end of path\n");
    __fat32_free_buffer(buf);
    return STATUS_OK;
} // create_mkdrip

// constucts a fat table of from the partition lba
int fat32_init(uint32_t partition_lba) {
    uint8_t buf[FAT32_SECTOR_SIZE];
    ata_read_sector(partition_lba, buf);
    const fat32_bpb_t *bpb = (fat32_bpb_t *)buf;

    if (bpb->bytes_per_sector != FAT32_SECTOR_SIZE) {
        DEBUG("[FAT32][INIT]: bytes_pre_sector lower than 512. Is: %d\n",
              bpb->bytes_per_sector);
    }

    f32_fs.partition_lba = partition_lba;
    f32_fs.fat_start     = partition_lba + bpb->reserved_sectors;
    f32_fs.data_start =
        f32_fs.fat_start + (bpb->fat_count * bpb->sectors_per_fat);
    f32_fs.root_cluster           = bpb->root_cluster;
    f32_fs.sectors_per_cluster    = bpb->sectors_per_cluster;
    f32_fs.bytes_per_sector       = bpb->bytes_per_sector;
    f32_fs.fat_count              = bpb->fat_count;
    f32_fs.sectors_per_fat        = bpb->sectors_per_fat;
    f32_fs.last_allocated_cluster = 0;
    uint32_t data_sectors = bpb->total_sectors_32 - bpb->reserved_sectors -
                            (bpb->fat_count * bpb->sectors_per_fat);
    f32_fs.maximum_cluster_size = (data_sectors / bpb->sectors_per_cluster) +
                                  2; // 2 for reserved clusters

    DEBUG("[FAT32][INIT]: partition_lba: %d\n", f32_fs.partition_lba);
    DEBUG("[FAT32][INIT]: fat_start: %d\n", f32_fs.fat_start);
    DEBUG("[FAT32][INIT]: data_start: %d\n", f32_fs.data_start);
    DEBUG("[FAT32][INIT]: root_cluster: %d\n", f32_fs.root_cluster);
    DEBUG("[FAT32][INIT]: sectors_per_cluster: %d\n",
          f32_fs.sectors_per_cluster);
    DEBUG("[FAT32][INIT]: bytes_per_sector: %d\n", f32_fs.bytes_per_sector);
    DEBUG("[FAT32][INIT]: fat_count: %d\n", f32_fs.fat_count);
    DEBUG("[FAT32][INIT]: sectors_per_fat %d\n", f32_fs.sectors_per_fat);
    DEBUG("[FAT32][INIT]: maximum cluster size: %d\n",
          f32_fs.maximum_cluster_size);

    return STATUS_OK;
} // init