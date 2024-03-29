#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>

#include "../include/aos_fs.h"

int print_superblock(int fd, int *d_blocks){
    ssize_t ret;
    struct aos_super_block aos_sb;

    ret = read(fd, (char *)&aos_sb, sizeof(aos_sb));
    if (ret != AOS_BLOCK_SIZE) {
        printf("Super block: read [%d] bytes from disk image.\n", (int)ret);
        return -1;
    }

    printf("Superblock: \n");
    printf("\tmagic: %lx\n", aos_sb.magic);
    printf("\tblock size: %lu\n", aos_sb.block_size);
    printf("\tdata block size: %lu\n", aos_sb.data_block_size);
    printf("\tpartition size: %lu\n", aos_sb.partition_size);
    printf("\tfirst: %lu\n", aos_sb.first);
    printf("\tlast: %lu\n", aos_sb.last);
    printf("\tfree blocks: %lx\n", aos_sb.padding[0]);

    *d_blocks = aos_sb.partition_size-2;
    return 0;
}

int print_inode(int fd){
    ssize_t ret;
    int nbytes;
    char *padding;
    struct aos_inode root_inode;

    ret = read(fd, (char *)&root_inode, sizeof(root_inode));
    if (ret != sizeof(root_inode)) {
        printf("Root inode: read [%d] bytes from disk image.\n", (int)ret);
        return -1;
    }

    printf("Root inode: \n");
    printf("\tprivileges: %u\n", root_inode.mode);
    printf("\tinode number: %lu\n", root_inode.inode_no);

    nbytes = AOS_BLOCK_SIZE - sizeof(root_inode);
    padding = malloc(nbytes);
    if(!padding) {
        perror("Malloc failed");
        return -1;
    }
    ret = read(fd, padding, nbytes); // add padding to device
    if (ret != nbytes) {
        printf("Inode store: read [%d] bytes of [%d] expected.\n", (int)ret, nbytes);
        return -1;
    }

    return 0;
}

int print_data_blocks(int fd, int nblocks){
    ssize_t ret;
    struct aos_data_block aos_block;
    int i;

    for (i = 0; i < nblocks; ++i) {
        ret = read(fd, (char*)&aos_block, sizeof(aos_block));
        if (ret != AOS_BLOCK_SIZE){
            printf("Data block [%d]: read [%d] bytes.\n", (int)ret, i+2);
            return -1;
        }
        printf("Data block [%d]: \n", i+2);
        printf("\tis valid: %lu\n", aos_block.metadata.is_valid);
        printf("\tprev: %lu\n", aos_block.metadata.prev);
        printf("\tnext: %lu\n", aos_block.metadata.next);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int fd, d_blocks;

    if (argc != 2) {
        printf("Usage: debug_fs <device>\n");
        return EXIT_FAILURE;
    }

    /* Open the device */
    if ((fd = open(argv[1], O_RDWR)) == -1) {
        perror("Error opening the device");
        return EXIT_FAILURE;
    }

    if(print_superblock(fd, &d_blocks) == -1) {
        close(fd);
        return EXIT_FAILURE;
    }

    if(print_inode(fd) == -1){
        close(fd);
        return EXIT_FAILURE;
    }

    if(print_data_blocks(fd, d_blocks) == -1){
        close(fd);
        return EXIT_FAILURE;
    }

    close(fd);
    return 0;
}

