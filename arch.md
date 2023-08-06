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

### Upgrade ###

Indirect blocks