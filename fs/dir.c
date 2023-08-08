#include <linux/module.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>

#include "include/aos_fs.h"

/*
 * This function iterates through the entries in a directory. It is called by the readdir() system call.
 * This function can return a number equal to the existing number of entries (1)
 * or 0, where there are no more entries to read.
 * The function will be called consecutively until all available entries are read.
 * */
static int aos_iterate(struct file *file, struct dir_context* ctx) {

    aos_fs_info_t *fs_info;
    struct aos_inode inode;
    loff_t pos;
    int i;
    int ret;


    /*
    // current directory
    if (ctx->pos == 0){
        if(!dir_emit(ctx, ".", FILENAME_MAXLEN, ROOT_INODE_NUMBER, DT_UNKNOWN)){
            return 0;
        } else{
            ctx->pos++;
        }
    }

    // parent directory
    if (ctx->pos == 1){
        if(!dir_emit(ctx,"..", FILENAME_MAXLEN, 1, DT_UNKNOWN)){
            return 0;
        } else {
            ctx->pos++;
        }
    }
     */
    if (!dir_emit_dots(file, ctx)) return -ENOSPC;

    /*
    // file
    if (ctx->pos == 2){
        if(!dir_emit(ctx, DEVICE_NAME, FILENAME_MAXLEN, FILE_INODE_NUMBER, DT_UNKNOWN)){
            return 0;
        } else{
            ctx->pos++;
        }
    }
    return 0;
     */

    fs_info = file->f_inode->i_sb->s_fs_info;

    pos = 1;
    for (i = 0; i < fs_info->sb->inodes_count; ++i){
        if (!(fs_info->sb->free_blocks >> i)) continue;
        pos++;
        if (ctx->pos == pos){
            if (!dir_emit(ctx, "", FILENAME_MAXLEN, file->f_inode->i_ino, DT_UNKNOWN)) return -ENOSPC;
            ctx->pos++;
        }

    }
    return 0;
}

const struct file_operations aos_dops = {
    .owner = THIS_MODULE,
    .iterate = aos_iterate
};

