/*
* UFS: our own Filesystem in Userspace 
* Copyright (c) 2009 LuQianhui <qianhui_lu@qq.com>
* All rights reserved.
*
* �ļ����ƣ�ufs.h
* ժ    Ҫ��ufs��ͷ�ļ����������ļ�ϵͳ����ز���           
*
* ��ǰ�汾��1.0
* ��    �ߣ�Ʈ����˿
* ������ڣ�2009��2��20��
*
*/

#ifndef UFS_H
#define UFS_H

#include <stddef.h>

#define DISK "/home/luqianhui/1/src/diskimg"  /* �����ļ�ϵͳ��Ϣ��ӳ���ļ���·������ʹ�þ���·��*/

#define MAX_BITMAP_IN_BLOCK 1280 /* λͼ�������*/
#define MAX_FILENAME 8           /* �ļ�������󳤶�*/
#define MAX_EXTENSION 3         /* �ļ���չ������󳤶� */
#define MAX_DATA_IN_BLOCK 504		/* ���ݿ������ɵ�������� the size of size_t and long is 4 seperately, 504=512-4-4 */
#define BLOCK_BYTES 512         /* ÿ�������*/       



long max_filesystem_in_block; 

typedef struct {
	 long fs_size;                     //size of file system, in blocks
	 long first_blk;                   //first block of root directory
	 long bitmap;                      //size of bitmap, in blocks
}sb;

typedef struct {
	 char fname[MAX_FILENAME + 1];      //filename (plus space for nul)
	 char fext[MAX_EXTENSION + 1];      //extension (plus space for nul)
	 size_t fsize;                      //file size, but for directory it doesn't need it
	 long nStartBlock;                  //where the first block is on disk
	 int flag;                          //indicate type of file. 0:for unused; 1:for file; 2:for directory
}file_directory;

typedef struct {
	 size_t size;                      // how many bytes are being used in this block
	 long nNextBlock;                  //The next disk block, if needed. This is the next pointer in the linked allocation list. else value -1
	 char data[MAX_DATA_IN_BLOCK];     // And all the rest of the space in the block can be used for actual data storage
}u_fs_disk_block ;

typedef long int ufs_DIR;              //the position value of dir_entry corresponds to the number of bytes 
									   //from the beginning of the image file
							  
#endif
