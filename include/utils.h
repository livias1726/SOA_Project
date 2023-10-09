#ifndef SOA_PROJECT_UTILS_H
#define SOA_PROJECT_UTILS_H

#define wake_on_bit(map, bit) \
    clear_bit(bit, map);      \
    wake_up_bit(map, bit);    \

int put_new_block(int blk, char* source, size_t size, int prev);
int invalidate_block(int blk);

static inline int get_blk(struct buffer_head **bh, struct super_block* sb, int blk, struct aos_data_block** db){

    *bh = sb_bread(sb, blk);
    if(!*bh) return -EIO;
    *db = (struct aos_data_block*)(*bh)->b_data;

    return 0;
}

static inline int cpy_blk(struct super_block* sb, int blk, int size, struct aos_data_block* db){
    struct buffer_head *bh;

    /* Read data block into a local variable */
    bh = sb_bread(sb, blk);
    if(!bh) return -EIO;
    memcpy(db, bh->b_data, size);
    brelse(bh);

    return 0;
}

#endif //SOA_PROJECT_UTILS_H
