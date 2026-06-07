#include "fat32.h"

/**
* fat32_write_file_at_offset - Writes data to given offset.
* @first_cluster: starting fluster of the file
* @offset: the data sector starting point
* @buf: -
* @size: how long of buffer length
*
* Description:
* -
*
* Context: Call when there is a file opened and want you want to write to it
* Returns: STATUS_ERROR || STATUS_OK
*/
int fat32_write_file_at_offset(uint32_t first_cluster, uint32_t offset, const uint8_t *buf, uint32_t size) {

    DEBUG("[FAT32][WRITE_AT_OFFSET]: input param: first_cluster: %d\n", first_cluster);
    DEBUG("[FAT32][WRITE_AT_OFFSET]: input param: offset %d\n", offset);
    DEBUG("[FAT32][WRITE_AT_OFFSET]: input size: %d\n", size);
    DEBUG("[FAT32][WRITE_AT_OFFSET]: input param: buf %s\n", buf);

    uint32_t target_cluster = first_cluster;
    uint32_t cluster_size   = __fat32_calculate_cluster_size();
    uint32_t cluster_chain_length = offset / cluster_size; //calculate the lenght of chain
    uint32_t target_offset  = offset % cluster_size; //

    DEBUG("[FAT32][WRITE_AT_OFFSET]: cluster_size: %d\n", cluster_size);
    DEBUG("[FAT32][WRITE_AT_OFFSET]: cluster_chain_length: %d\n", cluster_chain_length);
    DEBUG("[FAT32][WRITE_AT_OFFSET]: target_offset: %d\n", target_offset);
    
    //loop through clusers to get to the last cluster
    for(uint32_t i = 0; i < cluster_chain_length; i++) {
        uint32_t temp_cluster = __fat32_next_cluster(target_cluster);
        
        if(temp_cluster == INVALID_CLUSTER || temp_cluster >= FAT32_CLUSTER_EOC_MIN) {
            DEBUG("[FAT32][OFFSE_WRITE]: Cluster walk failed: %d\n", temp_cluster);
            return STATUS_ERROR;
        }
        
        target_cluster = temp_cluster;
    }

    uint8_t *tmp_buf = __fat32_allocate_buffer();
    if(tmp_buf == INVALID_BUFFER) return STATUS_ERROR;
    
    uint32_t bytes_written = 0;

    
    /*
    * Start by reading the target clusters contents to buffer.
    * looping around until every byte is written.
    */
    while(1) {
        if(__fat32_read_cluster(target_cluster, tmp_buf) == STATUS_ERROR) {
            DEBUG("[FAT32][OFFSE_WRITE]: Could not read the cluster\n");
            __fat32_free_buffer(tmp_buf);
            return STATUS_ERROR;
        }
        

        // Calculate remaining bytes inside the user data buffer
        uint32_t bytes_left = size - bytes_written;  
        // Calculate the maximum safe contiguous space remaining in the current active cluster block
        uint32_t space_in_cluster = cluster_size - target_offset;
        uint32_t bytes_to_write = (bytes_left < space_in_cluster) ? bytes_left : space_in_cluster;

        // Stage the incoming modifications inside our temporary write block
        memcpy(tmp_buf + target_offset, buf + bytes_written, bytes_to_write);

        //only write the safe amount of data to temporary_buffer
        
        //write the data to disk
        if(__fat32_write_cluster(target_cluster, tmp_buf) == STATUS_ERROR) {
            DEBUG("[FAT32][OFFSE_WRITE]: Could not write to cluster\n");
            __fat32_free_buffer(tmp_buf);
            return STATUS_ERROR;
        }

        //increase the size written and then check if it was over or the exact amount compared to the size.
        bytes_written += bytes_to_write;
        DEBUG("[FAT32][WRITE_AT_OFFSET]: bytes_written: %d\n", bytes_written);
        if(bytes_written >= size) {
            DEBUG("[FAT32][WRITE_AT_OFFSET]: Bytes_read higher or equal to size\n");
            __fat32_free_buffer(tmp_buf);
            return STATUS_OK;
        }

        /*
        * If everything was not written yet, change the target offset to 0 so on next iteratio we write starting from there
        * then get the next cluster. If next cluster is 0x0FFFFFF8 (FAT32_CLUSTER_EOC_MIN) and bytes left is higher than zero
        * allocate new cluster. Then chaining it to target and finally mark that as end of chain.
        * alternatively just use the next cluster
        */
        target_offset = 0;
        uint32_t next = __fat32_next_cluster(target_cluster);
        bytes_left    = size - bytes_written;
        DEBUG("[FAT32][WRITE_AT_OFFSET]: next cluster %d\n", next);
        
        
        if(next >= FAT32_CLUSTER_EOC_MIN && bytes_left > 0) {
            DEBUG("[FAT32][READ_FILE]: Reached end of cluster chain EOC\n");
            DEBUG("[FAT32][WRITE_AT_OFFSET]: allocating new cluster\n");
            uint32_t new_cluster = __fat32_alloc_cluster();

            if(new_cluster == INVALID_CLUSTER) {
                ERROR("[FAT32][READ_FILE]: Failed to alloc cluster\n");
                __fat32_free_buffer(tmp_buf);
                return STATUS_ERROR;
            }
            
            DEBUG("[FAT32][WRITE_AT_OFFSET]: chainig new cluster\n");
            if(__fat32_set_cluster(target_cluster, new_cluster) == STATUS_ERROR) {
                ERROR("[FAT32][READ_FILE]: Failed to chain clusters\n");
                __fat32_free_buffer(tmp_buf);
                return STATUS_ERROR;
            }
            
            DEBUG("[FAT32][WRITE_AT_OFFSET]: marking new cluster to be end of chain\n");
            if(__fat32_set_cluster(new_cluster, FAT32_CLUSTER_EOC) == STATUS_ERROR) {
                ERROR("[FAT32][READ_FILE]: Failed to set new cluster as end of chain\n");
                __fat32_free_buffer(tmp_buf);
                return STATUS_ERROR;
            }

            target_cluster = new_cluster;
        } else {
            target_cluster = next;
        }
    }

    __fat32_free_buffer(tmp_buf);
    return STATUS_ERROR;
}

/**
* fat32_write_file - Writes new file.
* @buf: data to write
* @size: buffer length
*
* Description:
* Writes the contents of a raw buffer sequentially into a series of newly allocated clusters.
*
* Context: Call when there is a file opened and want you want to write to it
* Returns: INVALID_CLUSTER || New files starting cluster number
*/
uint32_t fat32_write_file(const uint8_t *buf, uint32_t size) {

    uint32_t cluster_size = __fat32_calculate_cluster_size();
    
    // Ceiling division formula to determine the total number of blocks required for data
    uint32_t clusters_needed = (size + (cluster_size - 1)) / cluster_size;
    DEBUG("[FAT32][WRITE_FILE]: clusters needed for the file %d\n", clusters_needed);
    
    uint32_t first_cluster = 0;
    uint32_t prev_cluster = 0;

    // Allocate a temporary safety bounce buffer so our driver always writes exact cluster blocks
    uint8_t *temp_buffer = __fat32_allocate_buffer();
    if(temp_buffer == INVALID_BUFFER) return INVALID_CLUSTER;

    /*
    * Construct the file chain and commit the data payload blocks.
    */
    for(uint32_t i = 0; i < clusters_needed; i++) {
        uint32_t new_cluster = __fat32_alloc_cluster();
        if(new_cluster == INVALID_CLUSTER) {
            ERROR("[FAT32][WRITE_FILE]: Did not receive a cluster. Aborting\n");
            __fat32_free_buffer(temp_buffer);
            return INVALID_CLUSTER;
        }

        // Link the previously processed cluster forward to point to this new extension block
        if(prev_cluster != 0) {
            if(__fat32_set_cluster(prev_cluster, new_cluster) == STATUS_ERROR) {
                ERROR("[FAT32][WRITE_FILE]: Was not able to chain clusters. Aborting\n");
                __fat32_free_buffer(temp_buffer);
                return INVALID_CLUSTER;
            }
        }
        
        if(first_cluster == 0) first_cluster = new_cluster;

        // Calculate how much data remains inside our source pointer buffer
        uint32_t bytes_left = size - (i * cluster_size);
        uint32_t bytes_to_copy = (bytes_left > cluster_size) ? cluster_size : bytes_left;

        // Clean our scratch container and prime it with our exact chunk data fragment
        memset(temp_buffer, 0, cluster_size);
        memcpy(temp_buffer, buf + (i * cluster_size), bytes_to_copy);

        // Commit our zero-padded safe block straight to the physical storage sectors
        if(__fat32_write_cluster(new_cluster, temp_buffer) == STATUS_ERROR) {
            ERROR("[FAT32][WRITE_FILE]: Could not write to cluster. Aborting\n");
            __fat32_unalloc_cluster(new_cluster);
            __fat32_free_buffer(temp_buffer);
            return INVALID_CLUSTER;
        }

        prev_cluster = new_cluster;
    }

    /*
    * Mark the previous cluster as end of chain cause it can be the end of chain. 
    */
    if(prev_cluster != 0) {
        if(__fat32_set_cluster(prev_cluster, FAT32_CLUSTER_EOC) == STATUS_ERROR) {
            ERROR("[FAT32][WRITE_FILE]: Failed to write terminal EOC block marker\n");
            __fat32_free_buffer(temp_buffer);
            return INVALID_CLUSTER;
        }
    }

    __fat32_free_buffer(temp_buffer);
    return first_cluster;
}