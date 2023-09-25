# AOS FS #

## Layout ##

### Blocks ###

The filesystem handles NBLOCKS blocks of 4096 bytes each, where NBLOCKS is configured at compile time. These NBLOCKS
blocks are structured as follows:

* (0) Superblock. The first block of the disk is the superblock that describes the layout of the rest of the filesystem.
* (1, ..., NBLOCKS/10) Inode blocks. Ten percent of the total number of disk blocks are used as inode blocks, which contain inode data structures.
* (NBLOCKS/10 + 1, ..., NBLOCKS) Data blocks. The remaining blocks in the filesystem are used as data blocks.

#### Superblock ####

The fundamental information that the superblock stores are:
* The FS magic number
* The FS block size (4 kB by default)
* The number of blocks on the disk (NBLOCKS) 
* The number of blocks to store the inodes (10% of NBLOCKS by default)
* The number of inodes supported by the FS (AOS_MAX_INODES)
* The bit vectors representing which inodes and data blocks are free.

Other information that could be maintained in the superblock are:
* The maximum filename length
* The maximum file size (X bytes, subjected to the size of the file metadata, if a file must be on a single block)
* The FS root inode

#### Inode ####

The inode block is filled with contiguous aos_inode structures. The maximum possible number of inodes (AOS_MAX_INODES) 
must be limited to fill a single block size. Each aos_inode stores metadata about a file or directory and is associated 
with a single data block via data_block_number. The data block stores the file or directory contents. 
Each inode is indexed by its position, starting from 1 (functions in VFS that return inodes return 0 on error).

#### Data block ####

A data block can correspond to a directory or a regular file.

In the first case, a data block is a series of contiguous aos_dir_record structures.
The maximum possible number of records must be limited to fill a single block size.
The aos_dir_record structure maps a filename to the inode number. 

*has a flag indicating whether it’s active or not. When we unlink a file (rm command), we lazily delete the file by setting flag to 0.*

In the latter case, a data block is a stream of bytes representing file contents. 
Since each inode can have only one data block:
* files must be smaller than 4096 bytes.
* each directory can only have AOS_MAX_RECORDS children
* NBLOCKS <= AOS_MAX_INODES

# Concurrency #

## Seqlocks and Reader/Writer locks ##

### Seqlocks ### 

Readers never block a writer, but they must retry if a writer is in progress by detecting change in the sequence number.

### Atomic operations and bit flags ###


# System Calls #

## Put data ##

* Check device is mounted: if not, return ENODEV.
* Atomically increase the presence counter on the device.
* Check if the size is legal: if not, decrease the presence counter and return EINVAL.
* Find the first free block in the map: if not found, decrease the presence counter and return ENOMEM.
  * Atomically set to 1 the free block bit in the map: this is done in a loop of test_and_set, where if a concurrent PUT
    has selected the same free block, only one of them will be able to keep it (the first that set the bit).
    * Ensures that no PUT will operate on the same block concurrently.
* Allocate a data block to fill: if error, atomically clear the bit, decrease the presence counter and return ENOMEM.
* Fill data block.
* Retrieve buffer head for the specific block index: if error, free allocated memory, ..., and return EIO.
* Get the block seqlock as a writer.
* Copy the data block in the buffer head data.
* Release the block seqlock.
* Raise a new writing on buffer head to be written back in memory.
* Release the resources (buffer head, data block, presence counter).
* Return the used block index.

## Get data ##

* Check device is mounted: if not, return ENODEV.
* Atomically increase the presence counter on the device.
* Check if the size and the offset are legal: if not, decrease the presence counter and return EINVAL.
* Try reading the given block:
  * Retrieve buffer head for the specific block index: if error, decrease the presence counter and return EIO.
  * Copy block locally and release the buffer head.
* If the block is invalid, decrease the presence counter and return ENODATA.
* Copy the message from the block and retrieve its length compared to the size parameter.
* Deliver the message to the user, decrease the presence counter and return the number of bytes read.

## Invalidate data ##

* Check device is mounted: if not, return ENODEV.
* Atomically increase the presence counter on the device.
* Check if the offset is legal: if not, decrease the presence counter and return EINVAL.
* Try reading the given block: 
  * Retrieve buffer head for the specific block index: if error, decrease the presence counter and return EIO.
  * If the block is invalid, release the buffer head, decrease the presence counter and return ENODATA.
* Atomically clear the relative bit in the map.
* Get the block seqlock as a writer.
* Copy the data block in the buffer head data.
* Release the block seqlock.
* Raise a new writing on buffer head to be written back in memory.
* Release the resources (buffer head, data block, presence counter).
* Return the used block index.


# CHRONOLOGICAL READ

Using metadata: each block maintains a reference to its predecessor (to update 'last' when the last block of the
chronological chain is invalidated) and successor (to read chronologically)

## PUT DATA
1. Check device and params.
2. Test_and_Set the first free block. If no free blocks, return. -> No PUT on the same block.
3. Set PUT usage on blk_idx. If last is used by INV, then wait -> No INV conflicts
4. CAS on last and retrieve old_last. -> No conflicts on last for concurrent PUT.
5. Write new block (msg, is_valid, prev).
6. If old_val = 1, then change first = blk_idx.
7. Else, write preceding block (next).
8. Clear PUT usage on blk_idx.

## INVALIDATE DATA
1. Check device and params.
2. Test_and_Set INV usage on blk_idx -> if found set, then another INV on the same block is already pending.
   -> No INV on the same block.
3. If blk_idx is used by PUT, return. If blk_idx.prev is used by PUT, wait -> No PUT conflict.
4. Test_and_clear blk_idx on free block. If already cleared, return -> No INV on the same block. (REDUNDANT)
5. If blk_idx.prev and blk_idx.next are used by INV, wait -> No conflicts on concurrent INV.
6. Invalidate new block (is_valid).
7. If blk_idx = first AND blk_idx = last, CAS on last = 1 and return.
8. If blk_idx = first, CAS on first = blk_idx.next and return.
9. If blk_idx = last, CAS on last = blk_idx.prev and return.
10. Write preceding block (next = blk_idx.next) and successive block (prev = blk_idx.prev)
11. Clear blk_idx bit in INV_MASK.