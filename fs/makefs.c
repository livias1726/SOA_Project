#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "include/aos_fs.h"

/*
 * This user-level software will write the following information onto the disk
 * - BLOCK 0, superblock;
 * - BLOCK 1, inode of the unique file (the inode for root is volatile);
 * - BLOCK 2 ... , datablocks of the the device
*/

struct aos_super_block sb =
{
    .magic = MAGIC,
    .block_size = AOS_BLOCK_SIZE
};

struct aos_inode inode =
{
    .i_mode = S_IFREG,
    .i_ino = FILE_INODE_NUMBER
};

int main(int argc, char *argv[])
{
    int fd, nbytes;
    ssize_t ret;
    char *block_padding, *file_body = "Initialization content.\n";

    if (argc != 2) {
        printf("Usage: makefs <device>\n");
        return -1;
    }

    fd = open(argv[1], O_RDWR);
    if (fd == -1) {
        perror("Error opening the device");
        return -1;
    }

    // write super block
    ret = write(fd, (char *)&sb, sizeof(sb));
    if (ret != AOS_BLOCK_SIZE) {
        printf("Bytes written [%d] are not equal to the default block size.\n", (int)ret);
        close(fd);
        return (int)ret;
    }
    printf("Super block written successfully\n");

    // write inode
    inode.i_size = strlen(file_body);
    printf("File size is %ld\n",inode.i_size);
    fflush(stdout);
    ret = write(fd, (char *)&inode, sizeof(inode));
    if (ret != sizeof(inode)) {
        printf("The file inode was not written properly.\n");
        close(fd);
        return -1;
    }
    printf("File inode written successfully.\n");

    // padding for block 1
    // checkme: if it's better to add padding directly in the inode structure as in the superblock
    nbytes = AOS_BLOCK_SIZE - sizeof(inode);
    block_padding = malloc(nbytes);

    ret = write(fd, block_padding, nbytes);
    if (ret != nbytes) {
        printf("The padding bytes are not written properly. Retry your mkfs\n");
        close(fd);
        return -1;
    }
    printf("Padding in the inode block written successfully.\n");

    // write file datablock
    nbytes = (int)strlen(file_body);
    ret = write(fd, file_body, nbytes);
    if (ret != nbytes) {
        printf("Writing file datablock has failed.\n");
        close(fd);
        return -1;
    }
    printf("File datablock has been written successfully.\n");

    close(fd);

    return 0;
}
