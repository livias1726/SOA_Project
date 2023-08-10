#ifndef SOA_PROJECT_AOS_FS_H
#define SOA_PROJECT_AOS_FS_H

#include <linux/fs.h>
#include <linux/types.h>

#define MAGIC 0x42424242            // file system magic number
#define AOS_BLOCK_SIZE 4096
#define SUPER_BLOCK_IDX 0
#define INODE_BLOCK_IDX 1
//#define INODE_BLOCK_RATIO 0.10      // percentage of NBLOCKS reserved for inodes

#define FILENAME_MAXLEN 255
#define ROOT_INODE_NUMBER 10
#define FILE_INODE_NUMBER 1

#define DEVICE_NAME "the-device"
#define MODNAME "AOS_FS"

static DEFINE_MUTEX(device_state);  // used to prevent multiple access to the device

/* Superblock definition */
struct aos_super_block {
    uint64_t magic;             /* Magic number to identify the file system */
    uint64_t block_size;        /* Block size in bytes */
    uint64_t partition_size;    /* Number of blocks in the file system */
    uint64_t inode_blocks;      /* Number of blocks reserved for the inodes */
    uint64_t data_blocks;       /* Number of blocks used as data blocks */
    uint64_t inodes_count;      /* Number of inodes supported by the file system */
    uintptr_t *free_blocks;     /* Pointer to a bitmap to represent the state of each data block */

    // padding to fit into a single (first) block
    char padding[AOS_BLOCK_SIZE - (6 * sizeof(uint64_t) + sizeof(uintptr_t *))];
};

/* inode definition */
struct aos_inode {
    mode_t mode;                        /* File type and access rights */
    uint64_t inode_no;                  /* inode number */
    uint64_t data_block_number;         /* Data block number */
    uint64_t i_count;                   /* Usage counter */
    union {
        uint64_t file_size;             /* File size in bytes */
        uint64_t dir_children_count;    /* or dir size in number of files */
    };
};

// entries of a directory data block
struct aos_dir_record {
    char filename[FILENAME_MAXLEN];
    uint64_t inode_no;
};

/* Data block definition */
struct aos_db_metadata{
    uint8_t is_empty;
    uint8_t is_valid;
    uint16_t available_space;       /* in bytes */
};

struct aos_db_userdata{
    char msg[AOS_BLOCK_SIZE - (sizeof(struct aos_db_metadata))];
};

struct aos_data_block {
    struct aos_db_metadata metadata;
    struct aos_db_userdata data;
};

/* file system info */
typedef struct aos_fs_info {
    struct super_block *vfs_sb;     /* VFS super block structure */
    struct aos_super_block *sb;      /* AOS super block structure */
    uint8_t is_mounted;
} aos_fs_info_t;

extern const struct inode_operations aos_inode_ops;
extern const struct file_operations aos_file_ops;
extern const struct file_operations aos_dir_ops;
extern struct file_system_type aos_fs_type;

#endif //SOA_PROJECT_AOS_FS_H
