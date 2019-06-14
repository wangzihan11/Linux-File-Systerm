/*  
* UFS: our own Filesystem in Userspace 
* Copyright (c) 2009 LuQianhui <qianhui_lu@qq.com>
* All rights reserved.
*
* 文件名称：ufs.c
* 摘    要：实现FUSE提供的接口函数
* 	
* 
* 当前版本：1.0
* 作    者：飘零青丝
* 完成日期：2009年2月20日
*
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "ufs.h"
#include "op.h"

/**
 *  This function should look up the input path to
 *  determine if it is a directory or a file. If it is a
 *  directory, return the appropriate permissions. If it
 *  is a file, return the appropriate permissions as well
 *  as the actual size. This size must be accurate since
 *  it is used to determine EOF and thus read may not
 *  be called.
 * 
 *  return 0 on success, with a correctly set structure
 *  or -ENOENT if the file is not found
 */

static int u_fs_getattr(const char *path, struct stat *stbuf)
{

	u_fs_file_directory *attr=malloc(sizeof(u_fs_file_directory));
    memset(stbuf, 0, sizeof(struct stat));
    if(op_open(path,attr)==-1){
    	free(attr);
		return -ENOENT; 
    }
/* All files will be full access (i.e., chmod 0666), with permissions to be mainly ignored */
    if (attr->flag==2) {
    	stbuf->st_mode = S_IFDIR | 0666;
    } else if (attr->flag==1) {
        stbuf->st_mode = S_IFREG | 0666;
        stbuf->st_size = attr->fsize;
	}
	
	free(attr);
	return 0;

}

/**
 * This function should look up the input path, ensuring
 * that it is a directory, and then list the contents.
 * 
 * return 0 on success
 * or -ENOENT if the directory is not valid or found
 */
static int u_fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
	u_fs_disk_block *content=malloc(sizeof(u_fs_disk_block));
	u_fs_file_directory *attr=malloc(sizeof(u_fs_file_directory));
	if(op_open(path,attr)==-1){
		goto err;
	}
    long start_blk=attr->nStartBlock;
    if(attr->flag==1)
    	goto err;    	
    if(op_read_blk(start_blk, content))
		goto err;
		
	filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
       	
	u_fs_file_directory *dirent=(u_fs_file_directory*)content->data;
	int position=0;
	char name[MAX_FILENAME + MAX_EXTENSION + 2];		
	while( position<content->size ){/* the file pointer is not out of the content of direcotry */
	
		strcpy(name,dirent->fname);
		if(strlen(dirent->fext)!=0){
			strcat(name,".");
			strcat(name,dirent->fext);
		}		
        if (dirent->flag !=0 && filler(buf, name, NULL, 0))
            break;
		/* read next directory entry */
		dirent++;
		position+=sizeof(u_fs_file_directory);
	}
	
	free(attr);
	free(content);
	return 0;
err:
    free(attr);
    free(content);
    return -ENOENT;
}

/**
 * This function should not be modified, 
 * as you get the full path every
 * time any of the other functions are called.
 * 
 */
static int u_fs_open(const char *path, struct fuse_file_info *fi)
{   
    int res;
	u_fs_file_directory *attr=malloc(sizeof(u_fs_file_directory));
    res = op_open(path,attr);
    if (res == -1)
        return -errno;

    return 0;
   
}


/**
 * This function should add the new directory to the
 * root level, and should update the .directories file
 * 
 * return -ENAMETOOLONG if the name is beyond 8 chars
 * return -EPERM if the directory is not under the root dir only
 * return -EEXIST if the directory already exists
 */
static int u_fs_mkdir(const char *path, mode_t mode)
{
	int res=op_create(path, 2);
	return res;
}


/**
 * Deletes an empty directory
 * 
 * return 0 read on success
 * -ENOTEMPTY if the directory is not empty
 * -ENOENT if the directory is not found
 * -ENOTDIR if the path is not a directory
 */
static int u_fs_rmdir(const char *path)
{
	int res=op_rm(path,2);
    return res;
}

/**
 * This function should add a new file to a
 * subdirectory, and should update the .directories file
 * appropriately with the modified directory entry
 * structure.
 * 
 * return 0 on success
 * -ENAMETOOLONG if the name is beyond 8.3 chars
 * -EPERM if the file is trying to be created in the root dir
 * -EEXIST if the file already exists
 */
static int u_fs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res=op_create(path, 1);
	return res;
}

/**
 * This function should read the data in the file
 * denoted by path into buf, starting at offset.
 * 
 * reuturn size read on success
 * -EISDIR if the path is a directory
 * -ENOENT if the file not exist
 * -1 if error
 */ 
static int u_fs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
    
	u_fs_file_directory *attr=malloc(sizeof(u_fs_file_directory));
    if(op_open(path,attr)==-1) {
    	free(attr);
    	return -ENOENT;
    }
    if(attr->flag==2 ){
    	free(attr);
    	return -EISDIR;
    }
    
    u_fs_disk_block *content;    
    content=malloc(sizeof(u_fs_disk_block));
    
	if(op_read_blk(attr->nStartBlock, content)==-1){
		free(attr);
		free(content);
    	return -1;
	}	
	if( offset < attr->fsize){
		if (offset + size > attr->fsize )
             size = attr->fsize - offset;		
	} else
		size=0;
	int temp=size;
	char *pt=content->data;
	pt+=offset;
	strcpy(buf, pt);
	temp -= content->size;	
	while (temp > 0) {
		if(op_read_blk(content->nNextBlock, content)==-1)
			break;
		strcat(buf,content->data);	
		temp -= content->size;
	}
	free(content);
	free(attr);
    return size;
}

/**
 * This function should write the data in buf into the
 * file denoted by path, starting at offset.
 * 
 * return size on success
 * -EFBIG if the offset is beyond the file size (but handle appends)
 * -errno if else
 */ 
static int u_fs_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
	u_fs_file_directory *attr = malloc( sizeof(u_fs_file_directory));
	op_open(path,attr);
    long start_blk=attr->nStartBlock;
	
    if (start_blk == -1){
    	free(attr);
        return -errno;
    }   
    if( offset > attr->fsize){
    	free(attr);
    	return -EFBIG;
    }

	u_fs_disk_block *content = malloc( sizeof(u_fs_disk_block));
	int org_offset=offset;
	int ret=0;
	int total=0;
	long* next_blk=malloc(sizeof(long));
	int num;
	/* traverse the blocks where the file exist, find the block where the offset position lies*/
	while(1){
		if( op_read_blk(start_blk, content)==-1){
			size=-errno;
			goto exit;
		}
		if(offset<=content->size)
			break;		
		offset-=content->size;
		start_blk=content->nNextBlock;
	}
	
	char* pt = content->data;	
	pt+=offset;		
	ret = (MAX_DATA_IN_BLOCK - offset < size ? MAX_DATA_IN_BLOCK -offset : size); 		 	
	strncpy(pt,buf,ret);
	buf+=ret;
	content->size+=ret;
	total+=ret;
	size-=ret;
	if(size>0){
		/* find some free blocks to store the data */
 		num = op_search_free_blk( size/MAX_DATA_IN_BLOCK+1,next_blk);
		if( num == -1)
			return -errno;
		content->nNextBlock=*next_blk;
		op_write_blk(start_blk,content);
		int i;
		while(1){
			for(i=0; i<num; ++i){
				ret = (MAX_DATA_IN_BLOCK < size ? MAX_DATA_IN_BLOCK : size);
				content->size = ret;
				strncpy(content->data,buf,ret);
				buf+=ret;
				size-=ret;
				total+=ret;
				if(size==0)
					content->nNextBlock=-1;
				else
					content->nNextBlock=*next_blk+1;
				op_write_blk(*next_blk,content);
				*next_blk=*next_blk+1;
			}
			if(size==0)
				break;
			num = op_search_free_blk( size/MAX_DATA_IN_BLOCK+1,next_blk);
			if( num == -1)
				return -errno;																		
		}		
	}
	else if(size==0){
		/* the file size is less than before, some blocks should be free*/
		long next_blk;
		next_blk=content->nNextBlock;
		content->nNextBlock=-1;
		op_write_blk(start_blk,content);
		/* set the following block unused. */
		while(next_blk!=-1){
			op_set_blk(next_blk,0);
			op_read_blk(next_blk,content);
			next_blk=content->nNextBlock;
		}
	}
	size = total;
	
	/* modify the size of the file record. */
	attr->fsize = org_offset+size;
	if(op_setattr(path,attr)==-1){
		size=-errno;
		goto exit;
	}
	
exit: 
	free(attr);
	free(content);
//	free(next_blk);
	return size;
}

/**
 * Delete a file
 * 
 * return 0 read on success
 *-EISDIR if the path is a directory
 *-ENOENT if the file is not found
 */ 
static int u_fs_unlink(const char *path)
{
	int res=op_rm(path,1);
    return res;
}

/** 
 * This function should not be modified. 
 */
static int u_fs_truncate(const char *path, off_t size)
{
	return 0;
}
          
/** 
 * This function should not be modified. 
 */
static int u_fs_flush (const char * path, struct fuse_file_info * fi)
{
	return 0;
}

/**
 * This function initialize the filesystem when run,
 * read the supper block and 
 * set the global variables of the program.
 *
 */
void * u_fs_init (struct fuse_conn_info *conn){
	
	FILE * fp=NULL;

	fp=fopen(DISK, "r+");
	if(fp == NULL) {
		fprintf(stderr,"unsuccessful!\n");
		return 0;
	}
	/*  read the supper block and initilize the global variable*/
    sb *super_block_record=malloc(sizeof(sb));
    fread(super_block_record, sizeof(sb), 1, fp);
	max_filesystem_in_block = super_block_record->fs_size;
	fclose(fp);
	
	return (long*)max_filesystem_in_block;
}

static struct fuse_operations u_fs_oper = {
    .getattr	= u_fs_getattr,
    .readdir	= u_fs_readdir,
    .mkdir      = u_fs_mkdir,
    .rmdir      = u_fs_rmdir,
    .mknod      = u_fs_mknod,
    .read       = u_fs_read,
    .write      = u_fs_write,
	.unlink     = u_fs_unlink,    
    .open	    = u_fs_open,
    .flush	    = u_fs_flush,
	.truncate	= u_fs_truncate,
	.init       = u_fs_init,
};


int main(int argc, char *argv[])
{

    return fuse_main(argc, argv, &u_fs_oper, NULL);
}
