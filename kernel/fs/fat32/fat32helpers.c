#include "fat32.h"

/*
 * Calculate the Logical Block Address (LBA) of the FAT sector
 * that contains the allocation entry for the given cluster.
 */
uint32_t __fat32_calculate_lba(uint32_t cluster) {
    // DEBUG("[FAT32][CALCULATE_LBA]: calculating LBA for cluster %d\n",
    // cluster);
    if (cluster >= f32_fs.maximum_cluster_size) {
        ERROR("[FAT32][CALCULATE_LBA]: Cluster number too high: %d\n", cluster);
        return INVALID_LBA;
    }
    return f32_fs.fat_start +
           (cluster * FAT32_FAT_ENTRY_SIZE) / f32_fs.bytes_per_sector;
}

/*
 * Calculate the real sector number in the data region based on the given
 * cluster
 */
uint32_t __fat32_cluster_to_sector(uint32_t cluster) {
    if (cluster < 2) {
        ERROR("[FAT32][CLUSTER_TO_SECTOR]: Invalid cluster number: %d\n",
            cluster);
        return INVALID_CLUSTER;
    }
    return f32_fs.data_start + (cluster - 2) * f32_fs.sectors_per_cluster;
}

/*
 * Calculate the byte offset of the given cluster's entry within the FAT sector
 */
uint32_t __fat32_calculate_offset(uint32_t cluster) {
    if (cluster >= f32_fs.maximum_cluster_size) {
        ERROR("[FAT32][CALCULATE_OFFSET]: Cluster number too high: %d\n",
            cluster);
        return INVALID_CLUSTER;
    }
    return (cluster * FAT32_FAT_ENTRY_SIZE) % f32_fs.bytes_per_sector;
}

/*
 * Calculate the total size of a cluster in bytes
 * sector_per_cluster * (sector_size = 512)
 */
uint32_t __fat32_calculate_cluster_size() {
    return f32_fs.sectors_per_cluster * FAT32_SECTOR_SIZE;
}

/*
 * Calculate the maximum number of directory entries that can fit in a single
 * cluster (sector_per_cluster * (sector_size = 512)) / (dirent_size = 32);
 */
uint32_t __fat32_calculate_max_dir_entries() {
    return (f32_fs.sectors_per_cluster * FAT32_SECTOR_SIZE) / FAT32_DIRENT_SIZE;
}

/*
 *  An abstraction for kmalloc really. So many of the helpers and main functions
 * used these lines so I decided to make a dedicated helper func. Could be more
 * dynamic with a few addition but will do for now
 */
uint8_t *__fat32_allocate_buffer() {
    uint32_t cluster_size = __fat32_calculate_cluster_size();
    uint8_t *buf          = (uint8_t *)kmalloc(cluster_size);

    if (buf == NULL) {
        ERROR("[FAT32][ALLOC BUFFER]: Unable to allocate buffer\n");
        return INVALID_BUFFER;
    }

    return buf;
}

/**
 * __fat32_walk_dir_path() - Used to walk directory path.
 *
 * @param path: Pointer to path variable
 * @param name83: buffer where to save the last variable
 *
 * Description:
 * This func walks between / ... / and saves the formatted middle to name83
 *
 * Return: STATUS_ERROR || STATUS_OK.
 */
int __fat32_walk_dir_path(const char **path, uint8_t name83[11]) {
    const char *p = *path;

    while (*p == '/') { p++; }

    if (*p == '\0') {
        *path = p;
        return STATUS_ERROR;
    }

    char segment[13];
    uint8_t index = 0;

    while (index < 12) {
        if (p[index] == '\0' || p[index] == '/') {
            break;
        }
        segment[index] = p[index];
        index++;
    }
    segment[index] = '\0';
    *path          = p + index;

    DEBUG("[FAT32][MKDIRP]: Formatting current segment %s\n", segment);
    if (__fat32_format_83(segment, name83) == STATUS_ERROR) {
        ERROR("[FAT32][MKDIRP]: Could not format the name\n");
        return STATUS_ERROR;
    }

    return STATUS_OK;
}
/*
 * This function is useless might aswell just use kfree
 */
void __fat32_free_buffer(uint8_t *buf) {
    if (buf != NULL) {
        kfree(buf);
    }
}

/**
 * __fat32_next_cluster() - Used to walk in a specific the cluster chain.
 *
 * @param cluster: base cluster from which to start the walk
 *
 * Description:
 * Function calculates base clusters lba an then reads from disk
 * Then calculates the next entry number
 *
 * Return: INVALID_CLUSTER || next CLUSTER NUMBER.
 */
uint32_t __fat32_next_cluster(uint32_t cluster) {
    uint8_t *buf = __fat32_allocate_buffer();
    if (buf == INVALID_BUFFER)
        return INVALID_CLUSTER;

    /*
     * Start by calculating logical block address of the given FAT sector
     * and byte offset of the entry within that sector
     */
    uint32_t lba        = __fat32_calculate_lba(cluster);
    uint32_t fat_offset = __fat32_calculate_offset(cluster);

    // DEBUG("[FAT32][NEXT_CLUSTER]: finding next cluster with lba: %d\n", lba);
    // DEBUG("[FAT32][NEXT_CLUSTER]: offset: %d\n", fat_offset);

    /*
     *   Read the target lba to buffer from the actual hard drive.
     *   if read fails free the buffer and abort.
     */

    if (ata_read_sector(lba, buf) == STATUS_ERROR) {
        ERROR("[FAT32][NEXT_CLUSTER]: could not read sector with lba: %d\n",
            lba);
        __fat32_free_buffer(buf);
        return INVALID_CLUSTER;
    }

    /*
     *   cast buffer to to an uint32_t pointer so that
     *   we can calcualte the entry index within the previosly read logical
     * block address Then mask the entry with 0x0FFFFFFF to return only the
     * cluster number
     */

    const uint32_t *buf_ptr = (uint32_t *)buf;
    uint32_t entry          = buf_ptr[fat_offset / sizeof(uint32_t)];
    __fat32_free_buffer(buf);
    return (entry & FAT32_CLUSTER_MASK);
}

/**
 * __fat32_read_cluster() - Read data from disk to a buffer.
 *
 * @param cluster: cluster whose data will be read
 * @param buf:     buffer where the data will be read
 *
 * Description:
 * This functions calculates the sector number of the given cluster then reads
 * to given buffer the contents from disk
 *
 * Return: STATUS_ERROR || STATUS_OK.
 */
int __fat32_read_cluster(uint32_t cluster, uint8_t *buf) {
    // Get the real sector number of the given cluster
    uint32_t sector_number = __fat32_cluster_to_sector(cluster);
    if (sector_number == INVALID_CLUSTER)
        return STATUS_ERROR;

    /*
     * Loop as many times as there are sector in a cluster
     * Read the data to given buffer starting from calculated sector number.
     * Every iteration advance the sectors number by the iteration cycle number
     * and the buffer by iteration cycle * (FAT32_SECTOR_SIZE = 512) This turns
     * the cluster's contigous layout into a flat array of data more easily
     * manipulated.
     */
    for (uint8_t i = 0; i < f32_fs.sectors_per_cluster; i++) {
        if (ata_read_sector((sector_number + i),
                (buf + i * FAT32_SECTOR_SIZE)) == STATUS_ERROR) {
            ERROR("[FAT32][READ_CLUSTER]: reading clusters went wrong. "
                  "Aborting \n");
            return STATUS_ERROR;
        }
    }
    return STATUS_OK;
}

/**
 * __fat32_alloc_cluster() - Allocates next empty cluster from FAT table
 *
 * @param void:
 *
 * Description:
 * Allocation starts from the index of last allocated cluster and looks for next
 * free cluster from there on. No wrap around so if there are freed cluster
 * before it it will not find them.
 *
 * Return: INVALID_CLUSTER or CLUSTER NUMBER.
 */
uint32_t __fat32_alloc_cluster(void) {

    uint8_t *buf = __fat32_allocate_buffer();
    if (buf == INVALID_BUFFER)
        return INVALID_CLUSTER;

    // An optimization attempt. By saving the last allocated cluster we can
    // start the allocation from there by calculationg its sector index
    uint32_t starting_sector_index =
        f32_fs.last_allocated_cluster / FAT32_ENTRIES_PER_SECTOR;
    // Calculate entry index of the last allocated cluster
    uint32_t proposed_entry_index =
        f32_fs.last_allocated_cluster % FAT32_ENTRIES_PER_SECTOR;

    /*
     * Loop starting from the sector index until reached sectors per fat.
     */
    for (uint32_t i = starting_sector_index; i < f32_fs.sectors_per_fat; i++) {
        /*
         * Every loop read a sector to allocated buffer. Starting from fat_start
         * and advancing it by i
         *
         */
        if (ata_read_sector((f32_fs.fat_start + i), buf) == -1) {
            ERROR("[FAT32][ALLOC_CLUSTER]: Could read sector. Stopping cluster "
                  "allocation\n");
            __fat32_free_buffer(buf);
            return INVALID_CLUSTER;
        }

        /*
         * cast the buffer in to a uint32_t pointer array for ease of
         * manipulation fat_entries is an array fat entries
         */
        uint32_t *fat_entries = (uint32_t *)buf;

        /*
         * First iteration is the proposed entry index calculated before
         * starting the loop. Every other iteration use 0
         */
        uint32_t real_entry_index =
            (i == starting_sector_index) ? proposed_entry_index : 0;

        /*
         * Inner loop starting from the real enrty index until j reaches
         * (entries per sector = 512/4) if j is 0 or 1 go to next iteration
         * because the first 2 entries are reserved. by masking the value inside
         * the fat_entries[j] with 0x0FFFFFFF and checking if it then is same as
         * 0x00000000 (free cluster). If it is we have found a free cluster
         */
        for (uint32_t j = real_entry_index; j < FAT32_ENTRIES_PER_SECTOR; j++) {
            if (i == 0 && j < 2)
                continue;
            if ((fat_entries[j] & FAT32_CLUSTER_MASK) == FAT32_CLUSTER_FREE) {

                // calculate the absolute cluster number across the entire
                // filesystem
                uint32_t cluster_index = (i * FAT32_ENTRIES_PER_SECTOR) + j;

                // DEBUG("[FAT32][ALLOC_CLUSTER]: Free cluster found. Cluster "
                //    "index: %d\n",
                //     cluster_index);
                // mark index of current j as end of chain
                fat_entries[j] = FAT32_CLUSTER_EOC;

                /*
                 * Write to the hard drive starting from fat_start +
                 * current i the contents of the buf. Also write it to the
                 * second fat table Should writing fail mark the cluster
                 * of current J as free cluster and return Invalid
                 * cluster.
                 */
                if (ata_write_sector(f32_fs.fat_start + i, buf) == -1 ||
                    ata_write_sector(
                        f32_fs.fat_start + f32_fs.sectors_per_fat + i, buf) ==
                        -1) {
                    ERROR("[FAT32][ALLOC_CLUSTER]: Could not write to "
                          "sector. "
                          "Stopping cluster allocation\n");
                    fat_entries[j] = FAT32_CLUSTER_FREE;
                    __fat32_free_buffer(buf);
                    return INVALID_CLUSTER;
                }

                // save index of last allocated cluster so that next
                // allocation can continue from it.
                f32_fs.last_allocated_cluster = cluster_index;
                __fat32_free_buffer(buf);
                return cluster_index;
            }
        }
    }

    ERROR("[FAT32][ALLOC_CLUSTER]: Allocation failed. Could not find free "
          "spot\n");
    __fat32_free_buffer(buf);
    return INVALID_CLUSTER;
}

/**
 * __fat32_write_cluster() - Writes data to a disk.
 *
 * @param cluster: base cluster to write
 * @param buf:     holds what to write
 *
 * Description:
 * Calculates sector number of the given cluster and then writes
 * the content of buffer to there.
 *
 * Return: STATUS_ERROR || STATUS_OK
 */
int __fat32_write_cluster(uint32_t cluster, const uint8_t *buf) {
    // Start by calculating the clusters starting sector number in the data
    // region.
    uint32_t sector_number = __fat32_cluster_to_sector(cluster);
    if (sector_number == INVALID_CLUSTER)
        return STATUS_ERROR;

    /*
     * loop the numbers of times that there are sectors in cluster
     * while writing to hard drive starting from the sector number the contents
     * of the buffer Every iteration advances sector number by i and buffer by i
     * * (SECTOR_SIZE = 512) This writes the buffers data to contigous physical
     * sectors
     */
    for (int i = 0; i < f32_fs.sectors_per_cluster; i++) {
        if (ata_write_sector(sector_number + i, buf + i * FAT32_SECTOR_SIZE) ==
            -1) {
            ERROR("[FAT32][WRITE_CLUSTER]: Could not write to sector.\n");
            return STATUS_ERROR;
        }
    }
    return STATUS_OK;
}

/**
 * __fat32_set_cluster() - Updates clusters attributes
 *
 * @param cluster: cluster that will be updated
 * @param value:   holds the updated information
 *
 * Description:
 * When a specific cluster needs it values changed, it can be done using this
 * function for example it receives a cluster and as value it receives EOC.
 * Function then set That value to the cluster and writes it to disk
 *
 * Return: STATUS_ERROR || STATUS_OK.
 */
int __fat32_set_cluster(uint32_t cluster, uint32_t value) {

    uint8_t *buf = __fat32_allocate_buffer();
    if (buf == INVALID_BUFFER)
        return STATUS_ERROR;

    /*
     * Start by calculating logical block address of the given FAT sector
     * and byte offset of the entry within that sector
     */
    uint32_t lba        = __fat32_calculate_lba(cluster);
    uint32_t fat_offset = __fat32_calculate_offset(cluster);

    /*
     *   Read the target lba to buffer from the actual hard drive.
     *   if read fails free the buffer and abort.
     */

    if (ata_read_sector(lba, buf) == STATUS_ERROR) {
        ERROR("[FAT32][SET_CLUSTER]: Could not read sector with lba: %d\n",
            lba);
        __fat32_free_buffer(buf);
        return STATUS_ERROR;
    }

    /*
     *   cast buffer to an uint32_t pointer array.
     *   Then calculate the right index of the entry within read sector by
     * dividing the fat_offset by sizeof uint32_t Deposit the value to that
     * index.
     */

    uint32_t *buf_ptr                      = (uint32_t *)buf;
    buf_ptr[fat_offset / sizeof(uint32_t)] = value;

    /*
     * Write the change to the actual hard drive. Both the first and the second
     * fat tables.
     */
    if (ata_write_sector(lba, buf) == -1 ||
        ata_write_sector(lba + f32_fs.sectors_per_fat, buf) == -1) {
        __fat32_free_buffer(buf);
        return STATUS_ERROR;
    }

    __fat32_free_buffer(buf);
    return STATUS_OK;
}

/**
 * __fat32_unalloc_cluster()
 * might as well use set_cluster instead. This function is a stub, but lets see
 * if it can be used
 */
uint32_t __fat32_unalloc_cluster(uint32_t cluster) {
    if (__fat32_set_cluster(cluster, FAT32_CLUSTER_FREE) == STATUS_ERROR) {
        ERROR(
            "[FAT32][UNALLOC_CLUSTER]: Invalid cluster. Could not unallocate");
        return INVALID_CLUSTER;
    }
    return STATUS_OK;
}

/**
 * __fat32_format_83() - Formats fat 8.3 compatitable name to dst
 *
 * @param filename: name that requires formatting
 * @param dst:      formated name is copied to this
 *
 * Description:
 * Because we only have support for shorter name with 8 chars to name, 1 dot and
 * 3 for ext This function takes formats a single char at a time from the
 * filename turning it in upper case if it was not already. Never makes longer
 * name than 8 chars long leaves spaces if name or ext was shorter than 8 or 3.
 * If no dot in filename it formats only to 8 chars long and returns
 *
 * Return: STATUS_ERROR or STATUS_OK.
 */
int __fat32_format_83(const char *filename, uint8_t *dst) {

    if (filename == NULL) {
        return STATUS_ERROR;
    }

    // set dst to only contain spaces.
    memset(dst, FAT32_83_PAD, 11);

    /*
     * Try to find a dot within the filename
     */
    int index_of_dot = -1;
    for (uint8_t i = 0; i < strlen(filename); i++) {
        if (filename[i] == '.') {
            index_of_dot = i;
            break;
        }
    }

    // if there was no dot the filename it was a file or folder name. So no
    // extensions
    if (index_of_dot == -1) {
        for (uint8_t i = 0; i < strlen(filename); i++) {
            // filename lenght is at maxmimum 8 chars long.
            if (i == 8)
                break;

            // if the current char of the filename is between a and z
            // put them in to dst but change the into upper case. otherwise just
            // deposite
            if (filename[i] >= 'a' && filename[i] <= 'z') {
                dst[i] = filename[i] - ASCII_CASE_DIFF;
                continue;
            }
            dst[i] = filename[i];
        }
        return STATUS_OK;
    }

    // Deposit upercase chars to dst until index of dot is reached.
    for (uint8_t i = 0; i < index_of_dot; i++) {
        if (i == 8)
            break;
        if (filename[i] >= 'a' && filename[i] <= 'z') {
            dst[i] = filename[i] - ASCII_CASE_DIFF;
            continue;
        }
        dst[i] = filename[i];
    }

    // advance index of dot to the first char of the ext
    index_of_dot++;

    // turn the file ext tp upper chase adn deposit to dst. maximum lenght of
    // ext is 3, but can cap before that
    for (uint8_t i = 0; i < 3 && filename[index_of_dot + i] != '\0'; i++) {
        if (filename[index_of_dot + i] >= 'a' &&
            filename[index_of_dot + i] <= 'z') {
            dst[8 + i] = filename[index_of_dot + i] - ASCII_CASE_DIFF;
            continue;
        }
        // start depositing after the name
        dst[8 + i] = filename[index_of_dot + i];
    }

    return STATUS_OK;
}

/**
 * __fat32_search_dir() - Searches for a specific directory
 *
 * @param dir_cluster: starting directory cluster.
 * @param name83:      holds the name taht we are looking for
 * @param out_cluster: function places the found cluster in here
 * @param out_size:    function places found size in here
 * @param out_attr:    function places found attributes in here
 *
 * Description:
 * Function searches for a specific cluster starting from the given dir_cluster.
 * if it finds the dir in question it places its metadata to outgoing params.
 *
 * Return: STATUS_ERROR or STATUS_OK.
 */
int __fat32_search_dir(uint32_t dir_cluster, const uint8_t *name83,
    uint32_t *out_cluster, uint32_t *out_size,
    uint8_t *out_attr) {
    uint8_t *buf = __fat32_allocate_buffer();
    if (buf == INVALID_BUFFER)
        return STATUS_ERROR;

    uint8_t is_done          = 0;
    uint32_t current_cluster = dir_cluster;

    /*
     *   Start looping and read the data of the current cluster in to a buffer
     */
    while (is_done != 1) {
        if (__fat32_read_cluster(current_cluster, buf) == STATUS_ERROR) {
            ERROR("[FAT32][SEARCH_DIR]: Could not read cluster\n");
            __fat32_free_buffer(buf);
            return STATUS_ERROR;
        }

        // Convert the buffer to a directory entry array
        fat32_dirent_t *dir_entry = (fat32_dirent_t *)buf;
        uint32_t max_dir_entries  = __fat32_calculate_max_dir_entries();

        /*
         * Loop as many times as there are entries in a directory.
         * First check that if the directory entrys name is 0x00
         * (FAT32_DIRENT_FREE). If it is true set is done to ok. and break
         * because there are no more active entries in the directory. the if the
         * directory entry is 0xE5 (FAT32_DIRENT_DELETED) or its attributes are
         * 0x0F (FAT32_LONG_FILE_NAME) continue immidialety to next iteration.
         * There is no support for long filenames for now
         */
        for (uint32_t i = 0; i < max_dir_entries; i++) {
            if (dir_entry[i].name[0] == FAT32_DIRENT_FREE) {
                is_done = STATUS_OK;
                break;
            }

            if (dir_entry[i].name[0] == FAT32_DIRENT_DELETED)
                continue;
            if ((dir_entry[i].attributes & FAT32_LONG_FILE_NAME) ==
                FAT32_LONG_FILE_NAME)
                continue;

            // compare the given name to the dir_entrys name. If it a match then
            // we gound the right dir entry
            if (memcmp(dir_entry[i].name, name83, 11) == 0) {

                // Construct a whole cluster from cluster high and cluster low.
                // Sifht the higher cluster by 16 so that the lower goes to
                // shifted part
                *out_cluster = (dir_entry[i].cluster_high << 16) |
                               dir_entry[i].cluster_low;
                *out_size    = dir_entry[i].size;
                *out_attr    = dir_entry[i].attributes;
                // DEBUG("[FAT32][SEARCH_DIR]: file name %s\n",
                // dir_entry[i].name); DEBUG("[FAT32][SEARCH_DIR]: file
                // found!\n"); DEBUG("[FAT32][SEARCH_DIR]: cluster: %d\n",
                // *out_cluster); DEBUG("[FAT32][SEARCH_DIR]: size: %d\n",
                // *out_size);
                __fat32_free_buffer(buf);
                return STATUS_OK;
            }
        }

        if (is_done == STATUS_OK) {
            break;
        }

        /*
         * If we did not find the filename or did not reach end of the active
         * entries proceed to next cluster
         */
        uint32_t next_cluster = __fat32_next_cluster(current_cluster);
        // DEBUG("[FAT32][SEARCH_DIR]: Next cluster number: %d\n",
        // next_cluster);

        if (next_cluster >= FAT32_CLUSTER_EOC_MIN) {
            // DEBUG("[FAT32][SEARCH_DIR]: End of the cluster chain\n");
            __fat32_free_buffer(buf);
            return STATUS_ERROR;
        }
        current_cluster = next_cluster;
    }

    __fat32_free_buffer(buf);
    return STATUS_ERROR;
}