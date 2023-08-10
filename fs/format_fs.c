#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>

#include "include/aos_fs.h"
#define ROUND_UP(a,b) (a + b - 1) / b


/*
 * This user-level software will write the following information onto the disk
 * - BLOCK 0, super block;
 * - BLOCK 1, inodes block (the inode for root is volatile);
 * - BLOCK 2, ... , nblocks+2, data blocks of the device
*/

static int build_superblock(int fd, int nblocks){
    ssize_t ret;
    int ino_blocks = ROUND_UP(nblocks * sizeof(struct aos_inode), AOS_BLOCK_SIZE);

    struct aos_super_block aos_sb = {
            .magic = MAGIC,
            .block_size = AOS_BLOCK_SIZE,
            .partition_size = nblocks+ino_blocks+1,
            .inode_blocks = ino_blocks,
            .inodes_count = nblocks,
            .data_blocks = nblocks
    };

    ret = write(fd, (char *)&aos_sb, sizeof(aos_sb));
    if (ret != AOS_BLOCK_SIZE) {
        printf("SB: Bytes written [%d] are not equal to the default block size.\n", (int)ret);
        return -1;
    }

    printf("Superblock written successfully\n");
    return ino_blocks;
}

static int build_inode(int fd, char **padding, int nblocks){
    ssize_t ret;
    int nbytes;

    struct aos_inode root_inode = {
            .mode = S_IFDIR,
            .inode_no = ROOT_INODE_NUMBER,
            .data_block_number = nblocks + 1       // superblock + inode blocks
    };

    ret = write(fd, (char *)&root_inode, sizeof(root_inode));
    if (ret != sizeof(root_inode)) {
        printf("INO: Bytes written [%d] are not equal to the default inode size.\n", (int)ret);
        return -1;
    }

    // reserve memory for each inode block
    nbytes = (AOS_BLOCK_SIZE * nblocks) - sizeof(root_inode);
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
    struct aos_data_block *aos_block;
    int i;

    for (i = 0; i < nblocks; ++i) {
        aos_block = calloc(1,sizeof(aos_block));
        if (!aos_block){
            perror("Malloc failed");
            return -1;
        }

        aos_block->metadata.is_empty = 1;

        ret = write(fd, aos_block, sizeof(struct aos_data_block));
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
    int ino_blocks;

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

    /* Configure the superblock in a single disk block */
    ino_blocks = build_superblock(fd, nblocks);
    if(ino_blocks == -1) {
        close(fd);
        return EXIT_FAILURE;
    }

    /* Configure the Inode blocks */
    if(build_inode(fd, &block_padding, ino_blocks) == -1){
        close(fd);
        return EXIT_FAILURE;
    }

    /* Configure Data blocks */
    if(build_data_blocks(fd, nblocks) == -1){
        close(fd);
        //free(block_padding);
        return EXIT_FAILURE;
    }

    close(fd);
    return 0;
}
