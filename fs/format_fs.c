#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>

#include "include/aos_fs.h"

/*
 * This user-level software will write the following information onto the disk
 * - BLOCK 0, super block;
 * - BLOCK 1, inodes block (the inode for root is volatile);
 * - BLOCK 2, ... , nblocks+2, data blocks of the device
*/

int main(int argc, char *argv[])
{
    int fd, nbytes, nblocks;
    ssize_t ret;
    char *block_padding;
    struct aos_super_block aos_sb;
    struct aos_inode aos_inode;
    struct aos_data_block *aos_block;

    if (argc != 3) {
        printf("Usage: format_fs <device> <NBLOCKS>\n");
        return EXIT_FAILURE;
    }

    /* Open the device */
    if ((fd = open(argv[1], O_RDWR)) == -1) {
        perror("Error opening the device");
        return EXIT_FAILURE;
    }

    /* Retrieve NBLOCKS */
    if ((nblocks = atoi(argv[2])) < 1){
        printf("NBLOCKS must be at least 1\n");
        close(fd);
        return EXIT_FAILURE;
    }

    /* Configure Super block in a single block */
    aos_sb.magic = MAGIC;
    aos_sb.block_size = AOS_BLOCK_SIZE;
    aos_sb.partition_size = nblocks;
    ret = write(fd, (char *)&aos_sb, sizeof(aos_sb));
    if (ret != AOS_BLOCK_SIZE) {
        printf("SB: Bytes written [%d] are not equal to the default block size.\n", (int)ret);
        close(fd);
        return (int)ret;
    }

    /* Configure Inode store in a single block */
    ret = write(fd, (char *)&aos_inode, sizeof(aos_inode));
    if (ret != sizeof(aos_inode)) {
        printf("The file inode was not written properly.\n");
        close(fd);
        return EXIT_FAILURE;
    }
    nbytes = AOS_BLOCK_SIZE - sizeof(aos_inode);
    block_padding = malloc(nbytes);
    //printf("here\n");
    ret = write(fd, block_padding, nbytes); // add padding to block 1
    if (ret != nbytes) {
        printf("The padding bytes are not written properly. Retry your mkfs\n");
        close(fd);
        return EXIT_FAILURE;
    }

    /* Configure Data blocks */
    for (int i = 0; i < nblocks; ++i) {
        aos_block = calloc(1,sizeof(aos_block));
        if (!aos_block){
            perror("Malloc failed");
            free(block_padding);
            close(fd);
            return EXIT_FAILURE;
        }

        aos_block->metadata.is_empty = 1;

        ret = write(fd, aos_block, sizeof(struct aos_data_block));
        if (ret != AOS_BLOCK_SIZE){
            printf("DB: Bytes written [%d] are not equal to the default block size.\n", (int)ret);
            close(fd);
            return (int)ret;
        }
    }

    close(fd);
    return 0;
}
