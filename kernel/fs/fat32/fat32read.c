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
    uint32_t cluster_size    = __fat32_calculate_cluster_size();
    uint32_t current_cluster = start_cluster;

    DEBUG("[FAT32][READ_FILE]: input param: size: %d\n", size);
    DEBUG("[FAT32][READ_FILE]: ipnut param: starting cluster: %d\n",
        start_cluster);

    uint8_t *temp_buf = __fat32_allocate_buffer();
    if (temp_buf == INVALID_BUFFER)
        return STATUS_ERROR;

    /*
     * Read from the disk the data of the current cluster. Every iteration
     * advance the buffer by the size read on previous iteration.
     */
    while (1) {
        if (__fat32_read_cluster(current_cluster, temp_buf) == STATUS_ERROR) {
            ERROR("[FAT32][READ_FILE]: Error reading cluster\n");
            __fat32_free_buffer(temp_buf);
            return STATUS_ERROR;
        }

        /*
         * Calculate how many bytes are left to read in the overall file,
         * then decide whether to copy an entire cluster or just a small
         * remaining fragment.
         */
        uint32_t bytes_left = size - bytes_read;
        uint32_t bytes_to_copy =
            (bytes_left > cluster_size) ? cluster_size : bytes_left;

        // copy only safe amount of data to given buffer
        memcpy(buf + bytes_read, temp_buf, bytes_to_copy);

        bytes_read += bytes_to_copy;
        DEBUG("[FAT32][READ_FILE]: Loop bytes_left: %d\n", bytes_read);
        DEBUG("[FAT32][READ_FILE]: Loop bytes_read: %d\n", bytes_read);
        DEBUG("[FAT32][READ_FILE]: Loop bytes_to_copied: %d\n", bytes_to_copy);

        // if bytes read is higher or same as given size, meaning that we got
        // the exact size or read too far stop and return.
        if (bytes_read >= size) {
            DEBUG("[FAT32][READ_FILE]: Bytes read higher than size now.\n");
            DEBUG("[FAT32][READ_FILE]: Bytes read: %d\n", bytes_read);
            DEBUG("[FAT32][READ_FILE]: Bytes copied: %d\n", bytes_to_copy);
            __fat32_free_buffer(temp_buf);
            return STATUS_OK;
        }

        // File was not fully read still so continue trhough the chain
        uint32_t next = __fat32_next_cluster(current_cluster);

        if (next >= FAT32_CLUSTER_EOC_MIN) {
            ERROR("[FAT32][READ_FILE]: Reached end of cluster chain EOC\n");
            __fat32_free_buffer(temp_buf);
            return STATUS_ERROR;
        }
        current_cluster = next;
    }
    __fat32_free_buffer(temp_buf);
    return STATUS_OK;
} // read_file