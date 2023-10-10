#include <linux/seqlock.h>
#include <linux/buffer_head.h>

#include "../include/config.h"
#include "../include/aos_fs.h"
#include "../include/utils.h"

extern aos_fs_info_t *info;

/*
 * Opens the block with index 'blk' and updates the metadata pointing to its successor with 'next'.
 * */
static int change_block_next(int blk, int next){
    struct buffer_head *bh_prev;
    struct aos_data_block *prev_block;
    int fail;

    write_seqlock(&info->block_locks[blk]);

    fail = get_blk(&bh_prev, info->vfs_sb, blk, &prev_block);
    if (fail < 0) {
        write_sequnlock(&info->block_locks[blk]);
        return fail;
    }

    prev_block->metadata.next = next;

    mark_buffer_dirty(bh_prev);
    brelse(bh_prev);

    write_sequnlock(&info->block_locks[blk]);

    return fail;
}

/*
 * Opens the block with index 'blk' and updates the metadata pointing to its predecessor with 'prev'.
 * */
static int change_block_prev(int blk, int prev) {
    struct buffer_head *bh_next;
    struct aos_data_block *next_block;
    int fail;

    write_seqlock(&info->block_locks[blk]);

    fail = get_blk(&bh_next, info->vfs_sb, blk, &next_block);
    if (fail < 0) {
        write_sequnlock(&info->block_locks[blk]);
        return fail;
    }
    next_block->metadata.prev = prev;

    mark_buffer_dirty(bh_next);
    brelse(bh_next);

    write_sequnlock(&info->block_locks[blk]);

    return fail;
}

static inline int put_blk(int old_last, int size, char* source, struct buffer_head *bh, struct aos_data_block *data_block){
    size_t ret;

    /* Get the message from user */
    ret = copy_from_user(data_block->data.msg, source, size);
    size -= ret;

    /* Write the message on the block */
    data_block->data.msg[size+1] = '\0';
    data_block->metadata.is_valid = 1;
    data_block->metadata.prev = old_last;
    data_block->metadata.next = 0;

    /* Update the block on the device */
    mark_buffer_dirty(bh);
    WB { sync_dirty_buffer(bh); } // immediate synchronous write on the device

    return size;
}

/**
 * Inserts a new message in the given block, updating its metadata
 * */
int put_new_block(int blk, char* source, size_t size, int old_last){
    struct buffer_head *bh;
    struct aos_data_block *data_block;
    int res, old_first;
    uint64_t prev, next;

    if (old_last == 1) { /* The block is the first to be inserted: no need to lock */
        old_first = __atomic_exchange_n(&info->first, blk, __ATOMIC_RELAXED);

        write_seqlock(&info->block_locks[blk]);

        res = get_blk(&bh, info->vfs_sb, blk, &data_block);
        if (res < 0) goto failure_2;

    } else {
        /* Wait for the previous PUT to complete to update the metadata */
        res = wait_on_bit(info->put_map, old_last, TASK_INTERRUPTIBLE);
        if (res) return -EBUSY;

        write_seqlock(&info->block_locks[blk]);

        res = get_blk(&bh, info->vfs_sb, blk, &data_block);
        if(res < 0) goto failure_1;

        prev = data_block->metadata.prev;
        next = data_block->metadata.next;

        __sync_bool_compare_and_swap(&info->first, blk, next);

        if (next != 0) {
            if (prev != 1) {
                res = change_block_next(prev, next);
                if (res < 0) {
                    brelse(bh);
                    goto failure_1;
                }
            }

            res = change_block_prev(next, prev);
            if (res < 0) {
                brelse(bh);
                goto failure_1;
            }
        }

        res = change_block_next(old_last, blk);
        if (res < 0) {
            brelse(bh);
            goto failure_1;
        }
    }

    size = put_blk(old_last, size, source, bh, data_block);

    brelse(bh);
    write_sequnlock(&info->block_locks[blk]);

    return size;

    failure_2:
        __sync_val_compare_and_swap(&info->first, blk, old_first); // reset 'first'
    failure_1:
        write_sequnlock(&info->block_locks[blk]);
        return res;
}

int invalidate_block(int blk){

    struct buffer_head *bh;
    struct aos_data_block *data_block;
    int fail;
    uint64_t prev, next;
    bool is_last = false;

    write_seqlock(&info->block_locks[blk]);

    fail = get_blk(&bh, info->vfs_sb, blk, &data_block);
    if (fail < 0) {
        write_sequnlock(&info->block_locks[blk]);
        return fail;
    }

    if (!data_block->metadata.is_valid) {
        fail = -ENODATA;
        goto failure;
    }

    prev = data_block->metadata.prev;
    next = data_block->metadata.next;

    /* - If 'offset' is 'first', change 'first' to 'next'
     * - If 'offset' is 'first' AND 'last', change 'last' to 1
     * - If 'offset' is 'last', change 'last' to 'prev' */
    (__sync_bool_compare_and_swap(&info->first, blk, next)) ?
    __sync_bool_compare_and_swap(&info->last, blk, 1) : (is_last = __sync_bool_compare_and_swap(&info->last, blk, prev));

    data_block->metadata.is_valid = 0;
    if (is_last) data_block->metadata.next = 0;
    mark_buffer_dirty(bh);

failure:
    write_sequnlock(&info->block_locks[blk]);
    brelse(bh);

    return fail;
}

