#ifndef SOA_PROJECT_AOS_FS_H
#define SOA_PROJECT_AOS_FS_H

#include <linux/fs.h>
#include <linux/types.h>

#define MAGIC 0x42424242            // file system magic number
#define AOS_BLOCK_SIZE 4096
#define SUPER_BLOCK_IDX 0
#define INODE_BLOCK_IDX 1

#define AOS_MAX_INODES (AOS_BLOCK_SIZE / sizeof(struct aos_inode))
#define AOS_MAX_RECORDS (AOS_BLOCK_SIZE / sizeof(struct aos_dir_record))

#define FILENAME_MAXLEN 255
#define ROOT_INODE_NUMBER 10
#define FILE_INODE_NUMBER 1

#define DEVICE_NAME "the-device"

static int NBLOCKS;
module_param(NBLOCKS, int, S_IRUGO);

#define MODNAME "AOS_FS"

static DEFINE_MUTEX(device_state);  // used to prevent multiple access to the device

/* Superblock definition */
struct aos_super_block {
    uint64_t magic;             /* Magic number to identify the file system */
    uint64_t block_size;        /* Block size in bytes */
    uint64_t partition_size;    /* Number of blocks in the file system */
    uint64_t inodes_count;
    uint64_t free_blocks;

    // padding to fit into a single (first) block
    __attribute__((unused)) char padding[AOS_BLOCK_SIZE - (8 * sizeof(uint64_t))];
};

/*
 * Bit vectors declared by macros:
* DECLARE_BIT_VECTOR(free_inodes, PFS_MAX_INODES);
* DECLARE_BIT_VECTOR(free_data_blocks, PFS_MAX_INODES);
* We indicate that an inode or data block is occupied by setting the corresponding location in
* the bit vector to 1
 * */

/*
 * All information needed by the filesystem to handle a file is included in a data structure called an inode.
 */

/* inode definition */
struct aos_inode {
    umode_t i_mode;                             /* File type and access rights */
    uint64_t i_ino;                             /* inode number */
    atomic_t i_count;                           /* Usage counter */
    loff_t i_size;                              /* File length in bytes */
    struct inode_operations * i_op;             /* inode operations */
    struct file_operations * i_fop;             /* Default file operations */
};

// entries of a directory data block
struct aos_dir_record {
    char filename[FILENAME_MAXLEN];
    uint64_t inode_no;
};

/* file system info */
typedef struct aos_fs_info {
    struct super_block *vfs_sb;     /* VFS super block structure */
    struct aos_super_block *sb;      /* AOS super block structure */
    uint8_t is_mounted;
    uint8_t *free_blocks;
} aos_fs_info_t;

extern const struct inode_operations aos_iops;
extern const struct file_operations aos_fops;
extern const struct file_operations aos_dops;
static struct file_system_type aos_fs_type;

#endif //SOA_PROJECT_AOS_FS_H
