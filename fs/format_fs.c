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

static int build_superblock(int fd, int nblocks){
    ssize_t ret;

    struct aos_super_block aos_sb = {
            .magic = MAGIC,
            .block_size = AOS_BLOCK_SIZE,
            .partition_size = nblocks+2,
    };

    ret = write(fd, (char *)&aos_sb, sizeof(aos_sb));
    if (ret != AOS_BLOCK_SIZE) {
        printf("SB: Bytes written [%d] are not equal to the default block size.\n", (int)ret);
        return -1;
    }

    printf("Superblock written successfully\n");
    return 0;
}

static int build_inode(int fd, char **padding){
    ssize_t ret;
    int nbytes;

    struct aos_inode root_inode = {
            .mode = S_IFREG,
            .inode_no = FILE_INODE_NUMBER
    };

    ret = write(fd, (char *)&root_inode, sizeof(root_inode));
    if (ret != sizeof(root_inode)) {
        printf("INO: Bytes written [%d] are not equal to the default inode size.\n", (int)ret);
        return -1;
    }

    // reserve memory for each inode block
    nbytes = AOS_BLOCK_SIZE - sizeof(root_inode);
    *padding = malloc(nbytes);
    if(!*padding) {
        perror("Malloc failed");
        return -1;
    }

    ret = write(fd, *padding, nbytes); // add padding to device
    if (ret != nbytes) {
        printf("Written [%d] padding bytes of [%d] expected.\n", (int)ret, nbytes);
        free(*padding);
        return -1;
    }

    printf("Inode store written successfully\n");
    return 0;
}

int build_data_blocks(int fd, int nblocks){
    ssize_t ret;
    int i;

    struct aos_db_metadata metadata = { 0 };
    struct aos_db_userdata data = { 0 };
    struct aos_data_block block = {
            .metadata = metadata,
            .data = data,
    };

    for (i = 0; i < nblocks; ++i) {
        ret = write(fd, &block, sizeof(block));
        if (ret != AOS_BLOCK_SIZE){
            printf("DB: Bytes written [%d] are not equal to the default block size.\n", (int)ret);
            return -1;
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int fd, nblocks;
    char *block_padding;

    if (argc != 3) {
        printf("Usage: format_fs <device> <NBLOCKS>\n");
        goto failure_1;
    }

    /* Open the device */
    if ((fd = open(argv[1], O_RDWR)) == -1) {
        perror("Error opening the device");
        goto failure_1;
    }

    /* Retrieve NBLOCKS */
    if ((nblocks = atoi(argv[2])) < 1){
        printf("NBLOCKS must be at least 1\n");
        goto failure_2;
    }

    /* Configure the superblock in a single disk block */
    if(build_superblock(fd, nblocks)) goto failure_2;

    /* Configure the Inode blocks */
    if(build_inode(fd, &block_padding)) goto failure_2;

    /* Configure Data blocks */
    if(build_data_blocks(fd, nblocks)) goto failure_3;

    close(fd);
    return 0;

failure_3:
    free(block_padding);

failure_2:
    close(fd);

failure_1:
    return EXIT_FAILURE;
}
