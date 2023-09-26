#ifndef SOA_PROJECT_AOS_FS_H
#define SOA_PROJECT_AOS_FS_H

#ifdef __KERNEL__
#include "../../../../../../../usr/src/linux-headers-5.4.0-156-generic/include/linux/fs.h"
#include "../../../../../../../usr/src/linux-headers-5.4.0-156-generic/include/linux/types.h"
#include "../../../../../../../usr/src/linux-headers-5.4.0-156-generic/include/linux/spinlock.h"
#include "../../../../../../../usr/src/linux-headers-5.4.0-156-generic/include/linux/seqlock.h"
#include "../../../../../../../usr/src/linux-headers-5.4.0-156-generic/include/linux/slab.h"
#include "../../../../../../../usr/src/linux-headers-5.4.0-156-generic/include/linux/bitmap.h"
#endif

#define MAGIC 0x42424242
#define AOS_BLOCK_SIZE 4096
#define SUPER_BLOCK_IDX 0
#define INODE_BLOCK_IDX 1
#define FILENAME_MAXLEN 255
#define ROOT_INODE_NUMBER 10
#define FILE_INODE_NUMBER 1

#define STATUS_INIT 0x8000000000000000
#define check_mount if (!(info->state & (1L << 63))) return -ENODEV

#define DEVICE_NAME "the-device"
#define MODNAME "AOS"
#define AUDIT if(1)

/* Superblock definition */
struct aos_super_block {
    uint64_t magic;             /* Magic number to identify the file system */
    uint64_t block_size;        /* Block size in bytes */
    uint64_t data_block_size;   /* Block size in bytes */
    uint64_t partition_size;    /* Number of blocks in the file system */

    // padding to fit into a single (first) block
    char padding[AOS_BLOCK_SIZE - (4 * sizeof(uint64_t))];
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
    // todo: is_mounted can be signalled with a single bit - see if it can be merged with count in a variable "state"
    uint64_t state;             /* Represents the state of the device: 1 bit for mount status and 63 for usage count */
    //uint8_t is_mounted;         /* Signals that the state of the device */
    //uint64_t count;             /* Number of threads currently operating on the device */
    uint64_t first;             /* First valid block written chronologically */
    uint64_t last;              /* Last valid block written chronologically */
    //---------------------------------------------------------------------------
    ulong *free_blocks;         /* Pointer to a bitmap to represent the state of each data block */
    // todo: see if writers flags can be handled better
    ulong *put_map;             /* Pointer to a bitmap to signal a pending PUT on a given block */
    ulong *inv_map;             /* Pointer to a bitmap to signal a pending PUT on a given block */
    ulong inv_put_lock;      /* Change this atomically: this only needs 2 bits (1 for INV, 1 for PUT) */
    //------------------------------------------------------------------------
    seqlock_t *block_locks;
} aos_fs_info_t;
#endif

extern const struct inode_operations aos_inode_ops;
extern const struct file_operations aos_file_ops;
extern const struct file_operations aos_dir_ops;
extern struct file_system_type aos_fs_type;
static DECLARE_WAIT_QUEUE_HEAD(wq);

#endif //SOA_PROJECT_AOS_FS_H
