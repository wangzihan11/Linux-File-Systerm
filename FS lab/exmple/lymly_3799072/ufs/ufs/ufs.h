#ifndef UFS_H
#define UFS_H

#include <stddef.h>

/*the absolute path of the mapping file*/
#define DISK "/root/last/test13/diskimg" 
/*the number of the bitmapblocks*/
#define MAX_BITMAP_BLOCK_NUM 7 
/*the max length of the file name*/
#define MAX_FILENAME 12 
/*the max space for storing datas in data block512-8-8=496*/
#define MAX_DATA_IN_BLOCK 496 
/*the size of block*/
#define BLOCK_BYTES 512

long max_filesystem_in_block;

struct super_block {
	long fs_size;  /*the num means how many blocks the file system has*/
	long first_blk; /*the first block of the root directory*/
	long bitmap; /*the num of the bitmap blocks*/
};

struct u_fs_inode {
	char fname[MAX_FILENAME + 1]; /*the name of the file or directory*/
	size_t fsize; /*the size of the file, if it is a directory, this segment does not need*/
	long nStartBlock; /*the first block where the file stores*/
	int flag; /*the type of the inode, 0: unused; 1: file; 2:directory*/
};

struct u_fs_data_block {
	size_t size;/*the num of bytes have used in the block*/ 
	long nNextBlock;/*the next block num, if it does not need, set -1*/
	char data[MAX_DATA_IN_BLOCK];/*the rest space of the block used for data storage*/
};

#endif