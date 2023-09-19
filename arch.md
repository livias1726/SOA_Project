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

### Reader/Writer locks ###

rwlock_t is a multiple readers and single writer lock mechanism.

* Non-PREEMPT_RT kernels implement rwlock_t as a spinning lock and the suffix rules of spinlock_t apply accordingly. 
The implementation is fair, thus preventing writer starvation.

* PREEMPT_RT kernels map rwlock_t to a separate rt_mutex-based implementation, thus changing semantics:
  * All the spinlock_t changes also apply to rwlock_t. 
  * Because an rwlock_t writer cannot grant its priority to multiple readers, a preempted low-priority reader will 
  continue holding its lock, thus starving even high-priority writers. In contrast, because readers can grant their 
  priority to a writer, a preempted low-priority writer will have its priority boosted until it releases the lock, 
  thus preventing that writer from starving readers.

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
