#include <linux/init.h>
#include <linux/module.h>
#include <linux/timekeeping.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "include/aos_fs.h"

/**
 * The device should be mounted on whichever directory of the file system to enable the operations by threads.
 * It is assumed that the device driver can support a single mount at a time.
 * */

static struct super_operations aos_sbops = {
};

static struct dentry_operations aos_eops = {
};

/*
 * the fill_super() function is called to terminate the superblock initialization.
 * This initialization involves filling the struct super_block structure fields and
 * the initialization of the root directory inode
 * */
static int aos_fill_super(struct super_block *sb, void *data, int silent) {

    aos_fs_info_t *info;
    struct aos_super_block *aos_sb;
    struct inode *root_inode;
    struct buffer_head *bh;
    struct timespec64 curr_time;
    uint64_t magic;
    uint8_t *free_blocks;
    uint16_t i;

    /* Updating the VFS super_block */
    sb->s_magic = MAGIC;
    sb->s_blocksize = AOS_BLOCK_SIZE;
    sb->s_type = &aos_fs_type; // file_system_type
    sb->s_op = &aos_sbops; // super block operations

    if (!(bh = sb_bread(sb, SUPER_BLOCK_IDX))) return -EIO;
    memcpy(aos_sb, bh->b_data, AOS_BLOCK_SIZE);
    brelse(bh);
    if (aos_sb->magic != MAGIC) return -EINVAL;

    if (!(info = (aos_fs_info_t *)(kzalloc(sizeof(aos_fs_info_t), GFP_KERNEL)))) return -ENOMEM;

    /* Mark used blocks */
    free_blocks = (uint8_t *)(vmalloc(info->sb->partition_size));
    if (!free_blocks) return -ENOMEM;
    for (i = 0; i < info->sb->partition_size; i++){
        free_blocks[i] = 0;
    }

    info->vfs_sb = sb;
    info->sb = aos_sb;
    sb->s_fs_info = info;

    // get a VFS inode for the root directory
    root_inode = iget_locked(sb, 0);
    if (!root_inode) {
        vfree(info->free_blocks);
        kfree(info);
        return -ENOMEM;
    }

    if (root_inode->i_state & I_NEW) { // created a new inode
        root_inode->i_ino = ROOT_INODE_NUMBER;
        inode_init_owner(root_inode, NULL, S_IFDIR);
        root_inode->i_sb = sb;
        root_inode->i_op = &aos_iops;
        root_inode->i_fop = &aos_dops;
        root_inode->i_mode = (S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IXUSR | S_IXGRP | S_IXOTH);
        ktime_get_real_ts64(&curr_time);
        root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime = curr_time;

        // no inode from device is needed - the root of our file system is an in memory object
        root_inode->i_private = NULL;

        // unlock the inode to make it usable
        unlock_new_inode(root_inode);
    }

    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root) {
        iget_failed(root_inode);
        vfree(info->free_blocks);
        kfree(info);
        return -ENOMEM;
    }

    /* Make the VFS superblock point to the dentry. */
    sb->s_root->d_op = &aos_eops;

    info->is_mounted = 1;

    return 0;
}

/* Default mount operation as seen in singlefilefs */
/*
 * The maximum number of manageable blocks is a parameter NBLOCKS that can be configured at compile time.
 * If a block-device layout keeps more than NBLOCKS blocks, the mount operation of the device should fail.
 * */
struct dentry *aos_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data) {

    struct dentry *ret;

    // mount a filesystem residing on a block device
    ret = mount_bdev(fs_type, flags, dev_name, data, aos_fill_super);

    if (unlikely(IS_ERR(ret))) {
        printk("%s: mount failure", MODNAME);
    }else {
        printk("%s: file system '%s' mounted\n", MODNAME, dev_name);
    }
    return ret;
}

static struct file_system_type aos_fs_type = {
    .owner = THIS_MODULE,
    .name = "aos_fs",
    .mount = aos_mount
};
