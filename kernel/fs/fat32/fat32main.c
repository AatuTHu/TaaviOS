#include "ata.h"
#include "fat32.h"
#include "klog.h"
#include <stdint.h>

fat32_fs_t f32_fs;

/**
 * __fat32_find_cluster() - Finds the specific file.
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
int fat32_find_cluster(uint32_t starting_dir_cluster, const char *path, uint32_t *out_file_cluster, uint32_t *out_dir_cluster, uint32_t *out_size,
                       char *out_fname, uint8_t *out_attr) {

    if (path == NULL) {
        ERROR("[FAT32][FIND_CLUSTER]: Given path was invalid\n");
        return STATUS_ERROR;
    }

    uint32_t current_directory_cluster = starting_dir_cluster;
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
            ERROR("[FAT32][FIND_CLUSTER]: was Unable to format the filename\n");
            return STATUS_ERROR;
        }

        // Search the current directory cluster for the formatted 8.3 name
        if (__fat32_search_dir(current_directory_cluster, name83,
                               &found_cluster, &found_size,
                               &found_attr) == STATUS_ERROR) {
            ERROR("[FAT32][FIND_CLUSTER]: Unable to find the directory\n");
            return STATUS_ERROR;
        }

        // if the path is now pointing to a value '\0' we have
        // reached the end of the path and found what we were looking for.
        // however if it is a forward slash advance by one and mask the
        // attributes to see that it is a directory and not a file
        if (*path == '\0') {
            memcpy(out_fname, segment, 9);
            *out_file_cluster = found_cluster;
            *out_size         = found_size;
            *out_attr         = found_attr;
            *out_dir_cluster  = (current_directory_cluster == 0) ? f32_fs.root_cluster : current_directory_cluster;
            DEBUG_FAT32("[FAT32][FIND_CLUSTER]: Directory cluster found: %d\n", *out_file_cluster);
            DEBUG_FAT32("[FAT32][FIND_CLUSTER]: Size: %d\n", *out_size);
            DEBUG_FAT32("[FAT32][FIND_CLUSTER]: last segment: %s\n", out_fname);
            return STATUS_OK;
        }
        if (path[0] == '/')
            path++;
        // See if directory bit is present
        if (!(found_attr & FAT32_ATTR_DIRECTORY)) {
            ERROR("[FAT32][FIND_CLUSTER]: Found invalid attributes\n");
            return STATUS_ERROR;
        }
        // step further in to a subdirectory
        current_directory_cluster = found_cluster;
    }

    return STATUS_ERROR;
}

int fat32_find_parent_cluster(uint32_t oprhan_cluster, uint32_t *out_parent) {

    uint32_t current_directory_cluster = f32_fs.root_cluster;
    DEBUG_FAT32("[FAT32][FPC]: root_cluster : %d\n", f32_fs.root_cluster);
    uint8_t *buf = __fat32_allocate_buffer();
    if (buf == INVALID_BUFFER) {
        ERROR("[FAT32][FPC]: Buffer alloc failed\n");
        return STATUS_ERROR;
    }

    while (1) {
        if (__fat32_read_cluster(current_directory_cluster, buf) == STATUS_ERROR) {
            ERROR("[FAT32][FPC]: failed to read cluster\n");
            return STATUS_ERROR;
        }

        if (current_directory_cluster == oprhan_cluster) {
            DEBUG_FAT32("[FAT32][FPC]: Current clusters parent is the root cluster\n");
            return STATUS_OK;
        }

        fat32_dirent_t *dir_ent   = (fat32_dirent_t *)buf;
        uint32_t max_entry_length = __fat32_calculate_max_dir_entries();

        for (uint32_t i = 0; i < max_entry_length; i++) {

            uint32_t oprhan_low  = oprhan_cluster & 0xFFFF;
            uint32_t orphan_high = (oprhan_cluster >> 16) & 0xFFFF;

            if (dir_ent[i].cluster_high == orphan_high && dir_ent[i].cluster_low == oprhan_low) {
                DEBUG_FAT32("[FAT32][FPC]: Found %s\n", dir_ent[i].name);
                *out_parent = current_directory_cluster;
                return STATUS_OK;
            }
        }

        uint32_t next = __fat32_next_cluster(current_directory_cluster);

        if (next >= FAT32_CLUSTER_EOC_MIN) {
            ERROR("[FAT32][FPC]: Cluster was end of the chain\n");
            return STATUS_ERROR;
        }

        current_directory_cluster = next;
    }
}

// constucts a fat table of from the partition lba
int fat32_init(ata_drive_t *drive, uint32_t partition_lba) {
    uint8_t buf[FAT32_SECTOR_SIZE];
    ata_read_sector(drive, partition_lba, buf);
    const fat32_bpb_t *bpb = (fat32_bpb_t *)buf;

    if (bpb->bytes_per_sector != FAT32_SECTOR_SIZE) {
        DEBUG_FAT32("[FAT32][INIT]: bytes_pre_sector lower than 512. Is: %d\n", bpb->bytes_per_sector);
    }

    f32_fs.drive                  = drive;
    f32_fs.partition_lba          = partition_lba;
    f32_fs.fat_start              = partition_lba + bpb->reserved_sectors;
    f32_fs.data_start             = f32_fs.fat_start + (bpb->fat_count * bpb->sectors_per_fat);
    f32_fs.root_cluster           = bpb->root_cluster;
    f32_fs.sectors_per_cluster    = bpb->sectors_per_cluster;
    f32_fs.bytes_per_sector       = bpb->bytes_per_sector;
    f32_fs.fat_count              = bpb->fat_count;
    f32_fs.sectors_per_fat        = bpb->sectors_per_fat;
    f32_fs.last_allocated_cluster = 0;
    uint32_t data_sectors         = bpb->total_sectors_32 - bpb->reserved_sectors -
                                    (bpb->fat_count * bpb->sectors_per_fat);
    f32_fs.maximum_cluster_size   = (data_sectors / bpb->sectors_per_cluster) +
                                    2;

    DEBUG_FAT32("[FAT32][INIT]: partition_lba: %d\n", f32_fs.partition_lba);
    DEBUG_FAT32("[FAT32][INIT]: fat_start: %d\n", f32_fs.fat_start);
    DEBUG_FAT32("[FAT32][INIT]: data_start: %d\n", f32_fs.data_start);
    DEBUG_FAT32("[FAT32][INIT]: root_cluster: %d\n", f32_fs.root_cluster);
    DEBUG_FAT32("[FAT32][INIT]: sectors_per_cluster: %d\n", f32_fs.sectors_per_cluster);
    DEBUG_FAT32("[FAT32][INIT]: bytes_per_sector: %d\n", f32_fs.bytes_per_sector);
    DEBUG_FAT32("[FAT32][INIT]: fat_count: %d\n", f32_fs.fat_count);
    DEBUG_FAT32("[FAT32][INIT]: sectors_per_fat %d\n", f32_fs.sectors_per_fat);
    DEBUG_FAT32("[FAT32][INIT]: maximum cluster size: %d\n", f32_fs.maximum_cluster_size);

    return STATUS_OK;
} // init
