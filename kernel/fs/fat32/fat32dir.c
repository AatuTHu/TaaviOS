#include "ata.h"
#include "config.h"
#include "fat32.h"
#include "klog.h"
#include "kmalloc.h"
#include <stdint.h>

/**
* __fat32_list_dir() - lists all directories
*
* @cluster: starting directory cluster.

* Description:
* Function starts searching directories from the given cluster. Printing them
directly to screen
*
*/
void fat32_list_dir(uint32_t cluster, uint8_t *out_buf, uint32_t *out_size) {
    uint32_t current_cluster = cluster;
    uint8_t *buf             = __fat32_allocate_buffer();
    if (buf == INVALID_BUFFER)
        return;

    if (__fat32_read_cluster(current_cluster, buf) == STATUS_ERROR) {
        ERROR("[FAT32][LIST_DIR]: Could not read cluster. Aborting\n");
        kfree(buf);
        return;
    }

    const fat32_dirent_t *dir_entry = (fat32_dirent_t *)buf;
    uint32_t max_entries            = __fat32_calculate_max_dir_entries();

    *out_size                       = 0;
    out_buf[0]                      = '\0';

    for (uint32_t i = 0; i < max_entries; i++) {
        if (dir_entry[i].name[0] == FAT32_DIRENT_FREE)
            break;
        if (dir_entry[i].name[0] == FAT32_DIRENT_DELETED)
            continue;
        if ((dir_entry[i].attributes & FAT32_LONG_FILE_NAME) ==
            FAT32_LONG_FILE_NAME)
            continue;
        if (memcmp(dir_entry[i].name, ".          ", 11) == 0 ||
            memcmp(dir_entry[i].name, "..         ", 11) == 0)
            continue;

        char name_buf[12];
        memcpy(name_buf, dir_entry[i].name, 11);
        name_buf[11]      = '\0';

        uint32_t name_len = strlen(name_buf);
        memcpy(&out_buf[*out_size], name_buf, name_len);

        *out_size += name_len;
        out_buf[*out_size] = '\n';
        *out_size += 1;
        out_buf[*out_size] = '\0';

        DEBUG_FAT32("[FAT32][LIST_DIR]: name: %s\n", name_buf);
        DEBUG_FAT32("[FAT32][LIST_DIR]: out_size: %d\n", *out_size);
    }

    kfree(buf);
    return;
} // list_dir

/**
 * __fat32_create_mkdirp() - Creates directory entry.
 *
 * @param starting_cluster: starting directory cluster
 * @param path: directory path
 *
 * Description:
 * This function makes a chain of directories as long as the path is
 * Before making a new directory it check if on with the same name exsist
 * thus preventing a duplication.
 *
 * Return: STATUS_ERROR || STATUS_OK.
 */
int fat32_mkdirp(uint32_t parent_cluster, const char *path) {

    DEBUG_FAT32("[FAT32][MKDIRP]: Current parent_cluster %d\n", parent_cluster);
    DEBUG_FAT32("[FAT32][MKDIRP]: Current path %s\n", path);

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
            goto error_case;
        }

        fat32_dirent_t *dir_entry = (fat32_dirent_t *)buf;

        if (__fat32_walk_dir_path(&path, name83) == STATUS_ERROR) {
            DEBUG_FAT32("[FAT32][MKDIRP]: Problem in path walking");
            goto error_case;
        }

        int found = 0;
        for (uint32_t i = 0; i < max_entries; i++) {
            if (dir_entry[i].name[0] == FAT32_DIRENT_FREE ||
                dir_entry[i].name[0] == FAT32_DIRENT_DELETED)
                continue;

            if (memcmp(name83, dir_entry[i].name, 11) == 0 &&
                dir_entry[i].attributes & FAT32_ATTR_DIRECTORY) {
                DEBUG_FAT32("[FAT32][MKDIRP]: Match found %s\n", dir_entry[i].name);
                current_directory_cluster = (dir_entry[i].cluster_high << 16) |
                                            dir_entry[i].cluster_low;
                found                     = 1;
                break;
            }
        }

        if (found == 1) {
            DEBUG_FAT32("[FAT32][MKDIRP]: match found!\n");
            continue;
        }

        DEBUG_FAT32("[FAT32][MKDIRP]: There were no match in the directory\n");
        int slot_found = 0;
        for (uint32_t i = 0; i < max_entries; i++) {
            if (dir_entry[i].name[0] == FAT32_DIRENT_FREE ||
                dir_entry[i].name[0] == FAT32_DIRENT_DELETED) {
                uint32_t new_dir_cluster = __fat32_alloc_cluster();

                if (new_dir_cluster == INVALID_CLUSTER) {
                    ERROR("[FAT32][MKDIRP]: failed to allocate new cluster\n");
                    goto error_case;
                }

                DEBUG_FAT32("[FAT32][MKDIRP]: Chaining next cluster to be END OF CHAIN\n");
                if (__fat32_set_cluster(new_dir_cluster, FAT32_CLUSTER_EOC) ==
                    STATUS_ERROR) {
                    ERROR("[FAT32][MKDIRP]: failed to set cluster as EOC\n");
                    goto error_case;
                }

                memcpy(dir_entry[i].name, name83, 11);
                DEBUG_FAT32("[FAT32][MKDIRP]: New dir name %s\n", dir_entry[i].name);
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
                    goto error_case;
                }

                // Wipe buffer empty and then format cluster to be empty
                memset(buf, 0, f32_fs.sectors_per_cluster * FAT32_SECTOR_SIZE);
                if (__fat32_write_cluster(new_dir_cluster, buf) == STATUS_ERROR) {
                    ERROR("[FAT32][MKDIRP]: Failed to format cluster on the disk\n");
                    goto error_case;
                }

                current_directory_cluster = new_dir_cluster;
                slot_found                = 1;
                break;
            }
        }

        if (!slot_found) {
            current_directory_cluster = __fat32_link_cluster_chain(current_directory_cluster);
            if (current_directory_cluster == INVALID_CLUSTER) {
                goto error_case;
            }
        }
    }

    DEBUG_FAT32("[FAT32][MKDIRP]: end of path\n");
    kfree(buf);
    return STATUS_OK;

error_case:
    kfree(buf);
    return STATUS_ERROR;
} // create_mkdrip

/**
* __fat32_update_dirent_size() - Updates directory metadata.
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
int fat32_update_dirent_size(uint32_t starting_cluster, uint32_t file_cluster, uint32_t new_size) {

    DEBUG_FAT32("[FAT32][UPDATE_DIRENT_SIZE]: starting_cluster: %d\n", starting_cluster);
    DEBUG_FAT32("[FAT32][UPDATE_DIRENT_SIZE]: file_cluster: %d\n", file_cluster);
    DEBUG_FAT32("[FAT32][UPDATE_DIRENT_SIZE]: new_size: %d\n", new_size);

    uint8_t *buf = __fat32_allocate_buffer();
    if (buf == INVALID_BUFFER)
        return STATUS_ERROR;

    if (starting_cluster == 0) {
        starting_cluster = f32_fs.root_cluster;
    }

    uint32_t cluster_size        = f32_fs.sectors_per_cluster * FAT32_SECTOR_SIZE;
    uint32_t current_dir_cluster = starting_cluster;

    /*
     * Start the loop by reading data of the directory cluster
     */
    while (1) {
        if (__fat32_read_cluster(current_dir_cluster, buf) == STATUS_ERROR) {
            ERROR("[FAT32][UPDATE_DIRENT_SIZE]: could not read cluster\n");
            goto error_case;
        }

        /*
         * cast the read info to be a directory entry structure
         * calculate of many entry elements there can be in a directory are in a
         * directory
         */
        fat32_dirent_t *dir_entry = (fat32_dirent_t *)buf;
        uint32_t max_dir_entries  = cluster_size / FAT32_DIRENT_SIZE;

        DEBUG_FAT32("[FAT32][UPDATE_DIRENT_SIZE]: dir_enty: %s\n", dir_entry->name);
        DEBUG_FAT32("[FAT32][UPDATE_DIRENT_SIZE]: max_dir_entries: %d\n", max_dir_entries);

        /*
         * loop the size of dir_enry. At every iteration construct a cluster
         * from the high and low cluster parts Check if it is the same as the
         * file cluster in question. if it is update the current directory entry
         * with the give size Then write tha changes to hardware.
         */

        for (uint32_t i = 0; i < max_dir_entries; i++) {
            uint32_t whole_cluster = (dir_entry[i].cluster_high << 16) | dir_entry[i].cluster_low;
            // DEBUG_FAT32("[FAT32][UPDATE_DIRENT_SIZE]: whole cluster: %d\n", whole_cluster);
            if (whole_cluster == file_cluster) {
                DEBUG_FAT32("[FAT32][UPDATE_DIRENT_SIZE]: Right dir cluster found!\n");
                dir_entry[i].size = new_size;
                if (__fat32_write_cluster(current_dir_cluster, buf) ==
                    STATUS_ERROR) {
                    ERROR("[FAT32][UPDATE_DIRENT_SIZE]: Failed to write in the cluste.");
                    goto error_case;
                }
                DEBUG_FAT32("[FAT32][UPDATE_DIRENT_SIZE]: Update complete!\n");
                kfree(buf);
                return STATUS_OK;
            }
        }

        /*
         * The target file entry was not in this directory cluster block.
         * Follow the chain link to fetch the next directory cluster block from
         * the FAT.
         */

        uint32_t next = __fat32_next_cluster(current_dir_cluster);

        if (next >= FAT32_CLUSTER_EOC_MIN) {
            ERROR("[FAT32][UPDATE_DIRENT_SIZE]: Next cluster was end of the chain.");
            goto error_case;
        }

        current_dir_cluster = next;
    }

error_case:
    kfree(buf);
    return STATUS_ERROR;
} // update_dir_size

/**
 * __fat32_create_dirent() - Creates directory entry.
 *
 * @directory_cluster: starting directory cluster
 * @directory_name: directory path
 * @entry_attributes: attributes of the endy, (folder, fili etc)
 *
 * Description:
 * This function makes a chain of directories as long as the path is
 * Before making a new directory it check if on with the same name exsist
 * thus preventing a dublication.
 *
 * Return: STATUS_ERROR || STATUS_OK.
 */
int fat32_create_dirent(uint32_t directory_cluster, const char *directory_name, uint32_t entry_attributes) {

    DEBUG_FAT32("[FAT32][MKDIR]: starting cluster number: %d\n", directory_cluster);
    DEBUG_FAT32("[FAT32][MKDIR]: directory_name: %s\n", directory_name);

    uint8_t *buf = __fat32_allocate_buffer();
    if (buf == INVALID_BUFFER)
        return STATUS_ERROR;

    if (__fat32_read_cluster(directory_cluster, buf) == STATUS_ERROR) {
        ERROR("[FAT32][MKDIR]: Could not read cluster. Aborting\n");
        goto error_case;
    }

    fat32_dirent_t *dir_entry = (fat32_dirent_t *)buf;
    uint32_t max_entries      = __fat32_calculate_max_dir_entries();
    uint8_t name83[11];

    // A conviniant function even tho we are not walking path.  It removes the / and formats the node
    if (__fat32_walk_dir_path(&directory_name, name83) == STATUS_ERROR) {
        ERROR("[FAT32][MKDIR]: Could not format name. Aborting\n");
        goto error_case;
    }

    for (uint32_t i = 0; i < max_entries; i++) {
        if (dir_entry[i].name[0] == FAT32_DIRENT_FREE ||
            dir_entry[i].name[0] == FAT32_DIRENT_DELETED)
            continue;

        if (memcmp(name83, dir_entry[i].name, 11) == 0 &&
            dir_entry[i].attributes & FAT32_ATTR_DIRECTORY) {
            DEBUG_FAT32("[FAT32][MKDIR]: Dir exist %s\n", dir_entry[i].name);
            kfree(buf);
            return STATUS_OK;
        }
    }

    int succeeded = 0;
    for (uint32_t i = 0; i < max_entries; i++) {
        if (dir_entry[i].name[0] == FAT32_DIRENT_FREE ||
            dir_entry[i].name[0] == FAT32_DIRENT_DELETED) {
            uint32_t new_dir_cluster = __fat32_alloc_cluster();

            if (new_dir_cluster == INVALID_CLUSTER) {
                ERROR("[FAT32][MKDIR]: failed to allocate new cluster\n");
                goto error_case;
            }

            DEBUG_FAT32("[FAT32][MKDIR]: Chaining next cluster to be END OF CHAIN\n");
            if (__fat32_set_cluster(new_dir_cluster, FAT32_CLUSTER_EOC) ==
                STATUS_ERROR) {
                ERROR("[FAT32][MKDIR]: failed to set cluster as EOC\n");
                goto error_case;
            }

            memcpy(dir_entry[i].name, name83, 11);
            DEBUG_FAT32("[FAT32][MKDIR]: New dir name %s\n", dir_entry[i].name);
            memset(dir_entry[i].reserved, 0, sizeof(dir_entry[i].reserved));

            dir_entry[i].cluster_low  = new_dir_cluster & 0xFFFF;
            dir_entry[i].cluster_high = (new_dir_cluster >> 16) & 0xFFFF;
            dir_entry[i].attributes   = entry_attributes;
            dir_entry[i].size         = 0;
            dir_entry[i].time         = 0;
            dir_entry[i].date         = 0;
            if (__fat32_write_cluster(directory_cluster, buf) ==
                STATUS_ERROR) {
                ERROR("[FAT32][MKDIR]: Failed to write directory entry to "
                      "disk\n");
                goto error_case;
            }

            // Wipe buffer empty and then format cluster to be empty
            memset(buf, 0, f32_fs.sectors_per_cluster * FAT32_SECTOR_SIZE);
            if (__fat32_write_cluster(new_dir_cluster, buf) == STATUS_ERROR) {
                ERROR("[FAT32][MKDIR]: Failed to format cluster on the disk\n");
                goto error_case;
            }

            succeeded = 1;
            break;
        }
    }

    kfree(buf);
    if (succeeded) {
        return STATUS_OK;
    } else {
        return STATUS_ERROR;
    }

error_case:
    kfree(buf);
    return STATUS_ERROR;
}

int fat32_delete_dirent(uint32_t target_cluster) {

    uint32_t current_cluster = target_cluster;
    uint8_t *buf             = __fat32_allocate_buffer();
    if (buf == INVALID_BUFFER)
        return STATUS_ERROR;

    while (1) {
        if (__fat32_read_cluster(current_cluster, buf) == STATUS_ERROR) {
            ERROR("[FAT32][DELETE_DIRENT]: There was error with reading the clusters data\n");
            goto error_case;
        }

        fat32_dirent_t *dir_entry = (fat32_dirent_t *)buf;
        uint32_t dir_entries      = __fat32_calculate_max_dir_entries();

        for (uint32_t i = 0; i < dir_entries; i++) {
            dir_entry[i].name[0] = FAT32_DIRENT_FREE;
        }

        if (__fat32_write_cluster(current_cluster, buf) == STATUS_ERROR) {
            ERROR("Failed to write changes to disk\n");
            goto error_case;
        }

        uint32_t next_cluster = __fat32_next_cluster(current_cluster);

        if (next_cluster >= FAT32_CLUSTER_EOC_MIN) {
            DEBUG_FS_TASK("[FAT32][DELETE_DIRENT]: Next cluster was end of the chain.\n");
            kfree(buf);
            return STATUS_OK;
        }

        current_cluster = next_cluster;
    }

error_case:
    kfree(buf);
    return STATUS_ERROR;
}
