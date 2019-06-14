/*
* UFS: our own Filesystem in Userspace 
* Copyright (c) 2009 LuQianhui <qianhui_lu@qq.com>
* All rights reserved.
*
* 文件名称：op.h
* 摘    要：声明 ufs文件系统的底层操作函数和块操作函数
*           the block operation functions and
*           the low level operation functions of ufs filesystems
*
* 当前版本：1.0
* 作    者：飘零青丝
* 完成日期：2009年2月20日
*
*/

#ifndef OP_H
#define OP_H

#include "ufs.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* ----------------------------------------------------------- *
 * Basic operation API                                         *
 * ----------------------------------------------------------- */


/**
 * This function create a directory or file 
 *
 * @param org_path indicate the path of the file or directory
 * @param flag:  1 if a file, 2 if a directory
 *
 * return 0 if success
 * return -ENAMETOOLONG if the name is beyond the limit
 * return -EPERM if the directory is not under the root dir only 
 * return -EEXIST if the directory or the file already exists
 */
int op_create(const char *, int flag);

/**
 * 	The function open a file or directory.
 *
 *  @param org_path indicate the path of the file or directory
 *  @param attr will store the corresponding u_fs_directory_entry when return. 
 *
 *  @return 0 when success else return -1.
 */
int op_open(const char * org_path, file_directory *attr);

/**
 *  The function modify the file attributes accroding to the path.
 *  
 *  @param org_path indicate the path
 *  @param attr store the modified parameters of u_fs_directory_entry.
 * 
 *  @return 0 when success else return -1. 
 */
int op_setattr(const char* org_path, file_directory * attr);


/**
 * Deletes an empty directory or a file
 *
 * @param org_path indicate the path 
 * @param flag 1 indicate file, 2 indicates directory,
 *
 * return 0 read on success
 * -ENOTEMPTY if the directory is not empty
 * -ENOENT if the directory or the file is not found
 * -ENOTDIR if the path is not a directory when delete a directory
 * -EISDIR if the path is a directory when delete a file
 * -1 if else error
 */
int op_rm(const char *path,int flag);

/** 
 * This function check the directory block is emtpy or not.
 * 
 * @param:path indicate the directory block
 * @return 1 if empty 
 *         else 0 
 */
int is_empty_dir(const char* path);

/** 
 * This function read a whole block at a time.
 * 
 * @param blk indicate the block to be read
 * @param content store the data of the block 
 *
 * @return 0 on success else -1 
 */
int op_read_blk(long blk,u_fs_disk_block * content);

/** 
 * This function write a whole block at a time.
 * 
 * @param blk indicate the block to be written
 * @param content store the block data to be written
 *
 * @return 0 on success else -1 
 */
int op_write_blk(long blk,u_fs_disk_block * content);

/**
 * The function search the bitmap blocks to find the blocks
 * which has enough space for the content or the max space.
 * 
 * @param num means the needed number of block
 * @param start_blk store the start sequence of the free blocks when return
 *
 * @return the number of the continued free blocks.
 *         else -1 if not found
 */
int op_search_free_blk(int num,long* start_blk);

/**
 * The function set the block used or unused.
 * 
 * @param start_blk is the block to be set
 * @param flag indicate used or unused 
 *
 * @return 0 if success else -1 
 */
int op_set_blk(long blk,int flag);

#endif
