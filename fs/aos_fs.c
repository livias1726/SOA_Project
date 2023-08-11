#include <linux/init.h>
#include <linux/module.h>
#include <linux/timekeeping.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>

#include "include/aos_fs.h"

/**
 * The device should be mounted on whichever directory of the file system to enable the operations by threads.
 * It is assumed that the device driver can support a single mount at a time.
 * */

aos_fs_info_t* get_fs_info(){
    return &fs_info;
}

static struct super_operations aos_sb_ops = {
};

static struct dentry_operations aos_de_ops = {
};

static int init_fs_info(aos_fs_info_t *info, struct aos_super_block* aos_sb) {
    // build free blocks bitmap as an array of uint64_t
    int nblocks = aos_sb->partition_size;
    int nll = ROUND_UP(nblocks, 64); // number of uint64_t needed to represent nblocks
    info->free_blocks = kzalloc(nll/8, GFP_KERNEL);
    if (!info->free_blocks) return -ENOMEM;

    // set first two blocks as used (superblock and inode block)
    SET_BIT(info->free_blocks, 0);
    SET_BIT(info->free_blocks, 1);

    info->sb = aos_sb;
    info->vfs_sb->s_fs_info = info;

    spin_lock_init(&(info->fs_lock));

    return 0;
}

/*
 * This function is called to terminate the superblock initialization, which involves filling the
 * struct super_block structure fields and the initialization of the root directory inode.
 *
 * The maximum number of manageable blocks is a parameter NBLOCKS that can be configured at compile time.
 * If a block-device layout keeps more than NBLOCKS blocks, the mount operation of the device should fail.
 * */
static int aos_fill_super(struct super_block *sb, void *data, int silent) {

    aos_fs_info_t *info;
    struct aos_super_block *aos_sb;
    struct inode *root_inode;
    struct buffer_head *bh;
    struct timespec64 curr_time;
    uint64_t magic;
    int ret;

    /* Read VFS superblock */
    sb->s_magic = MAGIC;
    bh = sb_bread(sb, SUPER_BLOCK_IDX);
    if(!bh) {
        ret = -EIO;
        goto fill_fail_1;
    }
    aos_sb = (struct aos_super_block *)bh->b_data;
    magic = aos_sb->magic;
    brelse(bh);

    if(magic != sb->s_magic) {
        ret = -EBADF;
        goto fill_fail_1;
    }

    sb->s_type = &aos_fs_type; // file_system_type
    sb->s_op = &aos_sb_ops; // super block operations

    /* Prepare AOS FS info */
    info = (aos_fs_info_t *)(kzalloc(sizeof(aos_fs_info_t), GFP_KERNEL));
    if (!info) {
        ret = -ENOMEM;
        goto fill_fail_1;
    }
    info->vfs_sb = sb;

    if(!init_fs_info(info, aos_sb)) goto fill_fail_2;

    // get a VFS inode for the root directory
    root_inode = iget_locked(sb, SUPER_BLOCK_IDX);
    if (!root_inode) goto fill_fail_3;

    if (root_inode->i_state & I_NEW) { // created a new inode
        root_inode->i_ino = ROOT_INODE_NUMBER;
        inode_init_owner(root_inode, NULL, S_IFDIR);
        root_inode->i_sb = sb;
        root_inode->i_op = &aos_inode_ops;
        root_inode->i_fop = &aos_dir_ops;
        root_inode->i_mode = (S_IFDIR | S_IRWXU | S_IROTH | S_IXOTH);
        ktime_get_real_ts64(&curr_time);
        root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime = curr_time;

        // no inode from device is needed - the root of our file system is an in memory object
        root_inode->i_private = NULL;
    }

    /* Make the VFS superblock point to the dentry. */
    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root) {
        iget_failed(root_inode);
        goto fill_fail_3;
    }
    sb->s_root->d_op = &aos_de_ops;

    info->is_mounted = 1;
    fs_info = *info;

    // unlock the inode to make it usable
    unlock_new_inode(root_inode);

    return 0;

fill_fail_3:
    kfree(info->free_blocks);
fill_fail_2:
    kfree(info);
    ret = -ENOMEM;
fill_fail_1:
    return ret;
}

static void aos_kill_superblock(struct super_block *sb){
    // todo: wait for pending operations
    kfree(sb->s_fs_info);
    kill_block_super(sb);
}

struct dentry *aos_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data) {

    struct dentry *ret;

    ret = mount_bdev(fs_type, flags, dev_name, data, aos_fill_super);

    if (unlikely(IS_ERR(ret))) {
        printk(KERN_ERR "%s: mount failure", MODNAME);
    }else {
        printk(KERN_INFO "%s: file system mounted on [%s]\n", MODNAME, dev_name);
    }
    return ret;
}

struct file_system_type aos_fs_type = {
    .owner = THIS_MODULE,
    .name = "aos_fs",
    .mount = aos_mount,
    .kill_sb = aos_kill_superblock
};
