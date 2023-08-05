# AOS FS #

## Layout ##

### Blocks ###

The file system handles NBLOCKS (+2) blocks of 4096 bytes each. NBLOCKS is configured at compile time.
* (0) Superblock
* (1) Inode
* (2, ..., NBLOCKS+2) Data blocks

#### Superblock ####

The superblock stores the information needed for a mounted file system
* inode and blocks locations
* file system block size
* maximum filename length
* maximum file size
* the location of the root inode 
* bit vectors representing which inodes and data blocks are free.

#### Inode ####

The inode block is filled with contiguous aos_inode structures. 
The maximum possible number of inodes (AOS_MAX_INODES) must be limited to fill a single block size.
Each aos_inode stores metadata about a file or directory and is associated with a single data block via data_block_number. 
The data block stores the file or directory contents. 
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