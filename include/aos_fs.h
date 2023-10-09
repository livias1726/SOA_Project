#ifndef SOA_PROJECT_AOS_FS_H
#define SOA_PROJECT_AOS_FS_H

#ifdef __KERNEL__
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/seqlock.h>
#include <linux/slab.h>
#include <linux/bitmap.h>
#endif

#define MAGIC 0x42424242
#define AOS_BLOCK_SIZE 4096
#define SUPER_BLOCK_IDX 0
#define INODE_BLOCK_IDX 1
#define FILENAME_MAXLEN 255
#define ROOT_INODE_NUMBER 10
#define FILE_INODE_NUMBER 1
#define NBLOCKS 32384           /* Maximum number of manageable blocks as limited by the use of 'padding' */
#define DEVICE_NAME "the-device"
#define MODNAME "AOS"
#define AUDIT if(1)

#define check_mount if (!is_mounted) return -ENODEV
#define EXTRA_BITS(dim) (AOS_BLOCK_SIZE/sizeof(ulong) - ((dim) * (sizeof(uint64_t)/sizeof(ulong))))
#define MAX_READ (64 * 1024)

/* Superblock definition */
struct aos_super_block {
    uint64_t magic;             /* Magic number to identify the file system */
    uint64_t block_size;        /* Block size in bytes */
    uint64_t data_block_size;   /* Block size in bytes */
    uint64_t partition_size;    /* Number of blocks in the file system */
    uint64_t first;             /* First valid block to be restored when mounting */
    uint64_t last;              /* Last valid block to be restored when mounting */

    ulong padding[EXTRA_BITS(6)]; /* Padding to fit into a single block: used to save the free blocks bitmap */
};

/* inode definition */
struct aos_inode {
    mode_t mode;                        /* File type and access rights */
    uint64_t inode_no;                  /* inode number */
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
    uint64_t is_valid;
    uint64_t prev;
    uint64_t next;
};

struct aos_db_userdata{
    char msg[AOS_BLOCK_SIZE - (sizeof(struct aos_db_metadata))];
};

struct aos_data_block {
    struct aos_db_metadata metadata;
    struct aos_db_userdata data;
};

/* file system info */
#ifdef __KERNEL__
typedef struct aos_fs_info {
    struct super_block *vfs_sb; /* VFS super block structure */
    struct aos_super_block sb;  /* AOS super block structure */
    uint64_t counter;           /* Represents the usage count of the device */
    uint64_t first;             /* First valid block written chronologically */
    uint64_t last;              /* Last valid block written chronologically */
    //---------------------------------------------------------------------------
    ulong *free_blocks;         /* Pointer to a bitmap to represent the counter of each data block */
    ulong *put_map;             /* Pointer to a bitmap to signal a pending PUT on a given block */
    ulong *inv_map;             /* Pointer to a bitmap to signal a pending PUT on a given block */
} aos_fs_info_t;

static DECLARE_WAIT_QUEUE_HEAD(wq);
#endif

extern const struct inode_operations aos_inode_ops;
extern const struct file_operations aos_file_ops;
extern const struct file_operations aos_dir_ops;
extern struct file_system_type aos_fs_type;
extern uint64_t is_mounted;

#endif //SOA_PROJECT_AOS_FS_H
