#include "fat32.h"

/**
 * __fat32_read_file() - Read contents of a specific file
 *
 * @start_cluster: file starting point
 * @size: size of file
 * @buf: out going buffer
 *
 * Description:
 * Reads a data from a specific file amount of size to a buffer
 *
 * @return STATUS_ERROR || STATUS_OK
 */
int fat32_read_file(uint32_t start_cluster, uint32_t size, uint8_t *buf) {

    uint32_t bytes_read      = 0;
    uint32_t cluster_size    = f32_fs.sectors_per_cluster * FAT32_SECTOR_SIZE;
    uint32_t current_cluster = start_cluster;

    DEBUG_FAT32("[FAT32][READ_FILE]: input param: size: %d\n", size);
    DEBUG_FAT32("[FAT32][READ_FILE]: ipnut param: starting cluster: %d\n", start_cluster);

    uint8_t *temp_buf = __fat32_allocate_buffer();
    if (temp_buf == INVALID_BUFFER)
        return STATUS_ERROR;

    while (1) {
        if (__fat32_read_cluster(current_cluster, temp_buf) == STATUS_ERROR) {
            ERROR("[FAT32][READ_FILE]: Error reading cluster\n");
            kfree(temp_buf);
            return STATUS_ERROR;
        }

        uint32_t bytes_left    = size - bytes_read;
        uint32_t bytes_to_copy = (bytes_left > cluster_size) ? cluster_size : bytes_left;

        memcpy(buf + bytes_read, temp_buf, bytes_to_copy);

        bytes_read += bytes_to_copy;

        DEBUG_FAT32("[FAT32][READ_FILE]: Loop bytes_left: %d\n", size - bytes_read);
        DEBUG_FAT32("[FAT32][READ_FILE]: Loop bytes_read: %d\n", bytes_read);
        DEBUG_FAT32("[FAT32][READ_FILE]: Loop bytes_to_copied: %d\n", bytes_to_copy);

        if (bytes_read >= size) {
            DEBUG_FAT32("[FAT32][READ_FILE]: Bytes read higher or equal to size now.\n");
            break;
        }

        uint32_t next = __fat32_next_cluster(current_cluster);

        if (next >= FAT32_CLUSTER_EOC_MIN) {
            DEBUG_FAT32("[FAT32][READ_FILE]: Reached end of cluster chain EOC\n");
            kfree(temp_buf);
            return STATUS_OK;
        }
        current_cluster = next;
    }
    kfree(temp_buf);
    return STATUS_OK;
}
