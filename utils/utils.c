#include <linux/seqlock.h>
#include <linux/buffer_head.h>

#include "../include/config.h"
#include "../include/aos_fs.h"
#include "../include/utils.h"

extern aos_fs_info_t *info;

/*
 * Updates the 'prev' variable in the metadata of the given block.
 * Used by INV operations to logically remove a 'first' block from the chain of readable blocks.
 * */
static int change_block_prev(int blk, int prev_blk){
    struct buffer_head *bh_next;
    struct aos_data_block *next_block;
    int fail = 0;

    write_seqlock(&info->block_locks[blk]);

    bh_next = sb_bread(info->vfs_sb, blk);
    if (!bh_next) {
        fail = -EIO;
        goto failure;
    }
    next_block = (struct aos_data_block*)bh_next->b_data;

    next_block->metadata.prev = prev_blk;

    mark_buffer_dirty(bh_next);
    brelse(bh_next);

failure:
    write_sequnlock(&info->block_locks[blk]);
    return fail;
}

/*
 * Updates the 'next' variable in the metadata of the given block.
 * Used by:
 * - PUT operations to chronologically link a block to the next one.
 * - INV operations to logically remove a block from the chain of readable blocks.
 * */
static int change_block_next(int blk, int next_blk){
    struct buffer_head *bh_prev;
    struct aos_data_block *prev_block;
    int fail = 0;

    write_seqlock(&info->block_locks[blk]);

    bh_prev = sb_bread(info->vfs_sb, blk);
    if(!bh_prev) {
        fail = -EIO;
        goto failure;
    }
    prev_block = (struct aos_data_block*)bh_prev->b_data;

    prev_block->metadata.next = next_blk;

    mark_buffer_dirty(bh_prev);
    brelse(bh_prev);

failure:
    write_sequnlock(&info->block_locks[blk]);
    return fail;
}

/*
 * Updates both the 'prev' and 'next' variable in the metadata of the given block.
 * Used by INV operations to logically remove a block from the chain of readable blocks.
 * */
int change_blocks_metadata(int prev_blk, int next_blk){
    struct buffer_head *bh_next, *bh_prev;
    struct aos_data_block *next_block, *prev_block;
    int fail = 0;

    if (prev_blk == 1) {
        fail = change_block_prev(next_blk, 1);
        return fail;
    } else if (next_blk == 0) {
        fail = change_block_next(prev_blk, 0);
        return fail;
    }

    /* PREV */
    write_seqlock(&info->block_locks[prev_blk]);

    bh_prev = sb_bread(info->vfs_sb, prev_blk);
    if(!bh_prev) {
        fail = -EIO;
        goto failure_1;
    }
    prev_block = (struct aos_data_block*)bh_prev->b_data;

    /* NEXT */
    write_seqlock(&info->block_locks[next_blk]);

    bh_next = sb_bread(info->vfs_sb, next_blk);
    if (!bh_next) {
        fail = -EIO;
        goto failure_2;
    }
    next_block = (struct aos_data_block*)bh_next->b_data;

    next_block->metadata.prev = prev_blk;
    prev_block->metadata.next = next_blk;

    mark_buffer_dirty(bh_prev);
    mark_buffer_dirty(bh_next);

    brelse(bh_prev);
    brelse(bh_next);

    failure_2:
        write_sequnlock(&info->block_locks[next_blk]);
    failure_1:
        write_sequnlock(&info->block_locks[prev_blk]);
        return fail;
}

/**
 * Inserts a new message in the given block, updating its metadata
 * */
int put_new_block(int blk, char* source, size_t size, int prev, int *old_first){
    struct buffer_head *bh;
    struct aos_data_block *data_block;
    int res;
    size_t ret;

    write_seqlock(&info->block_locks[blk]);

    bh = sb_bread(info->vfs_sb, blk);
    if(!bh) {
        res = -EIO;
        goto failure_1;
    }
    data_block = (struct aos_data_block*)bh->b_data;

    /* If the block is the first to be written (prev is 1), then there's no need to update the previous one
     * to point to the new block. */
    if (prev == 1) {
        *old_first = __atomic_exchange_n(&info->first, blk, __ATOMIC_RELAXED);
        DEBUG { printk(KERN_DEBUG "%s: [put_data() - %d] Atomically swapped 'first' from %d to %d. \n",
                       MODNAME, current->pid, *old_first, blk); }
    } else{
        /* Writes on the 'next' metadata of the previous block (old_last) */
        res = change_block_next(prev, blk);
        if (res < 0) goto failure_2;
    }

    // Write block on device
    ret = copy_from_user(data_block->data.msg, source, size);
    size -= ret;
    data_block->data.msg[size+1] = '\0';
    data_block->metadata.is_valid = 1;
    data_block->metadata.prev = prev;

    mark_buffer_dirty(bh);
    WB { sync_dirty_buffer(bh); } // immediate synchronous write on the device

    res = size;

    failure_2:
        brelse(bh);
    failure_1:
        write_sequnlock(&info->block_locks[blk]);
        return res;
}

void invalidate_block(int blk, struct aos_data_block *data_block, struct buffer_head *bh){

    //write_seqlock(&info->block_locks[blk]);
    data_block->metadata.is_valid = 0;
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    //write_sequnlock(&info->block_locks[blk]);

    brelse(bh);
}

int wait_for_invalidation(){

}
