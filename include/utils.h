#ifndef SOA_PROJECT_UTILS_H
#define SOA_PROJECT_UTILS_H

#define wake_on_bit(map, bit) \
    clear_bit(bit, map);      \
    wake_up_bit(map, bit);    \

#define check_wait(w,err,label) if(w){err = -EAGAIN; goto label;}

int put_new_block(int blk, char* source, size_t size, int prev, int *old_first);
int change_blocks_metadata(int prev_blk, int next_blk);
void invalidate_block(int blk, struct aos_data_block *data_block, struct buffer_head *bh);

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

#endif //SOA_PROJECT_UTILS_H
