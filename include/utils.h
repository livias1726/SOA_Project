#ifndef SOA_PROJECT_UTILS_H
#define SOA_PROJECT_UTILS_H

#define wake_on_bit(map, bit) \
    clear_bit(bit, map);      \
    wake_up_bit(map, bit);    \

#define check_wait(w,err,label) if(w){err = -EAGAIN; goto label;}

int put_new_block(int blk, char* source, size_t size, int prev);
int change_blocks_metadata(int prev_blk, int next_blk);
int change_block_next(int blk, int next_blk);
int invalidate_block(int blk);

static inline int wait_inv(ulong *map, int bit){
    bool wait;
    int trials = 0;
    do{
        wait = wait_on_bit_timeout(map, bit, TASK_INTERRUPTIBLE, JIFFIES);
        if (trials > SYSCALL_TRIALS) return -EAGAIN;
        trials++;
    } while (wait);

    return 0;
}

static inline int get_blk(struct buffer_head *bh, struct super_block* sb, int blk, struct aos_data_block** db){

    bh = sb_bread(sb, blk);
    if(!bh) return -EIO;
    *db = (struct aos_data_block*)bh->b_data;

    return 0;
}

#endif //SOA_PROJECT_UTILS_H
