#include <linux/module.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>

#include "../include/aos_fs.h"

/*
 * This function iterates through the entries in a directory. It is called by the readdir() system call.
 * This function can return a number equal to the existing number of entries (1)
 * or 0, where there are no more entries to read.
 * The function will be called consecutively until all available entries are read.
 * */
static int aos_iterate(struct file *file, struct dir_context* ctx) {

    if (!dir_emit_dots(file, ctx)) return -ENOSPC;

    // file
    if (ctx->pos == 2){
        if(!dir_emit(ctx, DEVICE_NAME, FILENAME_MAXLEN, FILE_INODE_NUMBER, DT_UNKNOWN)){
            return 0;
        } else{
            ctx->pos++;
        }
    }
    return 0;
}

const struct file_operations aos_dir_ops = {
    .owner = THIS_MODULE,
    .iterate = aos_iterate
};

