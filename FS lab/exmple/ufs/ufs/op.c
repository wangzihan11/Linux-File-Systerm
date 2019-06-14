/*
* UFS: our own Filesystem in Userspace 
* Copyright (c) 2009 LuQianhui <qianhui_lu@qq.com>
* All rights reserved.
*
* 文件名称：op.c
* 摘    要：实现ufs文件系统的底层操作函数和块操作函数
*           implment the block operation functions and
*           the low level operation functions of ufs filesystems
*
* 当前版本：1.0
* 作    者：飘零青丝
* 完成日期：2009年2月20日
*
*/

#include "ufs.h"
#include "op.h" 

#include <fcntl.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>    /* 消除警告：隐式声明与内建函数   ‘malloc’   不兼容  */
#include <errno.h>

/**
 *  The function modify the file attributes accroding to the path.
 *  
 *  @param org_path indicate the path
 *  @param attr store the modified parameters of u_fs_directory_entry. 
 *  @return 0 when success else return -1.
 * 
 */
int op_setattr(const char* org_path, u_fs_file_directory* attr)
{
	int res;
	char* path;
	char *p,*q;
	u_fs_disk_block* content;
	u_fs_file_directory *a=malloc(sizeof(u_fs_file_directory));
	content = malloc(sizeof(u_fs_disk_block)); 	
	path = strdup(org_path);	
	p=path;
	/* insufficient memory was available */
	if (!p){
		res = -1;
		goto exit;
	}
	p++;
	/* Find the end of the first token. */		
	q = strchr(p, '/');		
	if( q != NULL ){
		*q = '\0'; /* path points to the directory name*/
		q++;
		p=q;      /* p points to the file name */
		if(op_open(path,a)==-1){
			res = -ENOENT;
			goto exit;
		}
	}
	else {
		if(op_open("/",a)==-1){
			res = -ENOENT;
			goto exit;
		}
    }
		
	q = strchr(p, '.');
	if( q!=NULL ){ /* the file has extention name */
		*q = '\0'; 
		q++;     /* q points to the extention name */
	}			


    long start_blk=a->nStartBlock;

	if(start_blk==-1){
		res = -1;
		goto exit;
	}	
	if(op_read_blk(start_blk, content)==-1){
		res = -1;
		goto exit;
	}
	u_fs_file_directory *dirent=(u_fs_file_directory*)content->data;
	int position=0;	
	while( position<content->size ){/* the file pointer is not out of the content of direcotry */
		
		if( dirent->flag !=0 && strcmp(p,dirent->fname)==0 && ( q==NULL || strcmp(q,dirent->fext)==0 ) ){ /* find the entry */
			strcpy(dirent->fname, attr->fname);
			strcpy(dirent->fext, attr->fext); 
			dirent->fsize = attr->fsize;
			dirent->nStartBlock = attr->nStartBlock;
			dirent->flag = attr->flag;
			res=0;
			break;
		}
		/* read next directory entry */
		dirent++;
		position+=sizeof(u_fs_file_directory);
	}
	if( op_write_blk(start_blk, content)==-1){
		res = -1;
		goto exit;
	}
exit:
	free(a);
	free(path);
	free(content);
	return res;			
}

/**
 * 	The function open a file or directory.
 *
 *  @param org_path indicate the path of the file or directory
 *  @param attr will store the corresponding u_fs_directory_entry when return. 
 *
 *  @return 0 when success else return -1.
 */
int op_open(const char * org_path, u_fs_file_directory *attr){
		
	char *p,*q,*path;
	long start_blk;
	sb* sb_record;
	u_fs_disk_block *content;    
    content=malloc(sizeof(u_fs_disk_block));
    
	path = strdup(org_path);
	p=path;		

	/* read the super block*/
	if(op_read_blk(0,content)==-1)
		goto err; 	      	
    sb_record=(sb*)content;    
    start_blk=sb_record->first_blk;
    
	/* open root directory */
    if(strcmp(org_path,"/")==0){ 
		attr->flag=2;
		attr->nStartBlock=start_blk;
		goto ok;
	}
		
	//insufficient memory was available
	if (!p)
		goto err;		
	p++;
	/* Find the end of the first token. */		
	q = strchr(p, '/');		
	if( q!=NULL ){
		path++;  /* path points to the directory name */
		*q = '\0'; 
		q++;
		p=q;      /* p points to the file name */
	}
	
	q = strchr(p, '.');
	if( q!=NULL ){ /* the file has extention name */
		*q = '\0'; 
		q++;     /* q points to the extention name */
	}
	 
	 /* read the root directory block */ 
    if(op_read_blk(start_blk,content)==-1){
		goto err;
	}
	u_fs_file_directory *dirent=(u_fs_file_directory*)content->data;
		
	if(*path=='/')
		goto file; /* the file lies in root directory */
	int offset=0;	
	while( offset<content->size ){/* the file pointer is not out of the content of root direcotry */
		
		if(strcmp(dirent->fname,path)==0 && dirent->flag==2 ){/* the directory is found */
			start_blk=dirent->nStartBlock;
			break;
		}
		/* read next directory entry */
		dirent++;
		offset+=sizeof(u_fs_file_directory);
	}
	if(offset==content->size)
		goto err;
	
	if(op_read_blk(start_blk, content)==-1)
		goto err;	
	
	dirent=(u_fs_file_directory*)content->data;
	
file:
	offset=0;		
	while( offset<content->size ){/* the file pointer is not out of the content of root direcotry */
		
		if( dirent->flag !=0 && strcmp(dirent->fname,p)==0 && ( q==NULL || strcmp(dirent->fext,q)==0 ) ){ //find!
			 
			start_blk=dirent->nStartBlock;
			strcpy(attr->fname, dirent->fname);
			strcpy(attr->fext, dirent->fext);
			attr->fsize=dirent->fsize;
			attr->nStartBlock=dirent->nStartBlock;
			attr->flag=dirent->flag;
			
			goto ok;
		}
		/* read next directory entry */
		dirent++;
		offset+=sizeof(u_fs_file_directory);
	}	
	goto err;
ok:
		free(content);
//   	free(path);
		return 0;
err:
    	free(content);
//    	free(path);
    	return -1;

}


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
int op_create(const char* org_path, int flag){
	long blk;
	char *p,*q,*path;
	
	path = strdup(org_path);
	p=path;
	/* insufficient memory was available */
	if (!p)
		return -errno;
	
	u_fs_file_directory* attr=malloc(sizeof(u_fs_file_directory));
		
	/* Remove leading /'s. */
	p++;
	/* Find the end of the first token. */		
	q = strchr(p, '/');
	
	if(flag==2 && q!=NULL) /* if the directory is not under the root dir only  */
		return -EPERM;	
	else if( q!=NULL ){
		*q = '\0';	/* path points to the directory path */
		q++;
		p=q;      /* p points to file name */
		if(op_open(path,attr)==-1){
			free(attr);
			return -ENOENT;
		}
	}

	if(q==NULL){ /* the file lies in the root directory */
		if(op_open("/",attr)==-1){
			free(attr);
			return -ENOENT;
		}
    }
	
	if(flag==1){/* if it is file */
		q = strchr(p, '.');
		if( q!=NULL ){ // the file has extention name
			*q = '\0'; 
			q++;     // q points to the extention name
		}	
	}
	/*if the name is beyond the limit */
	if(flag==1){		
		if( strlen(p) > MAX_FILENAME 
		||  ( q != NULL && strlen(q) > MAX_EXTENSION) ){
			free(attr);	
			return -ENAMETOOLONG; 
		}
	}
	else if( flag==2 ){
		if(strlen(p)>MAX_FILENAME ){
			free(attr);
			return -ENAMETOOLONG;
		} 
	}
	
	long p_dir;
	/*open parent directory*/
	p_dir=attr->nStartBlock;
	free(attr);	
	
	if(p_dir == -1)
		return -ENOENT;
	
	u_fs_disk_block *content=malloc(sizeof(u_fs_disk_block));
	if(op_read_blk(p_dir, content)==-1)
		return -ENOENT;	
	u_fs_file_directory *dirent=(u_fs_file_directory*)content->data;
	
	int offset=0;
	int position=content->size;

	while( offset<content->size ){/* the file pointer is not out of the content of direcotry */
		if(flag==0)
			position=offset;		
		else if( flag==1 &&  dirent->flag==1 && strcmp(p,dirent->fname)==0 
		         && ( (q ==NULL && strlen(dirent->fext)==0 ) || (q!=NULL && strcmp(q,dirent->fext)==0) ) )
				return -EEXIST;
		else if ( flag==2 && dirent->flag==2 && strcmp(p,dirent->fname)==0 ) 
			   	return -EEXIST;
		
		/* read next directory entry */
		dirent++;
		offset+=sizeof(u_fs_file_directory);
	}
		      
	if(position==content->size){ /* enlarge the size to add a u_fs_file_directory record */
		if(content->size > MAX_DATA_IN_BLOCK){
			goto new_blk;			
		}
		else{
			content->size+=sizeof(u_fs_file_directory); 
		}	
	}
	else{
		offset=0;
		dirent=(u_fs_file_directory*)content->data;
		while(offset<position)
			dirent++;
	}
	strcpy(dirent->fname,p);
	if(flag ==1 && q!=NULL)
		strcpy(dirent->fext,q);
	dirent->fsize=0;
	dirent->flag=flag;
	long *temp=malloc(sizeof(long));	
	if(op_search_free_blk(1,temp)==1)
		dirent->nStartBlock=*temp;
	else
		return -errno;
	free(temp);	
    /* write back to the .directory block */
	op_write_blk(p_dir,content); 

exit:	
	/* initialize the file or the directory block */
	content->size=0;
	content->nNextBlock=-1;
	strcpy(content->data,"\0");	
	op_write_blk(dirent->nStartBlock,content);
    
    free(content);
    free(path);    
	return 0;

new_blk:
	
	if(op_search_free_blk(1,temp)==1)
		blk=*temp;
	else
		return -errno;
	free(temp);
	content->nNextBlock=blk;
	/* write back to the .directory block */
	op_write_blk(p_dir,content);
	/* initialize the new .directory block */
	content->size=sizeof(u_fs_file_directory);
	content->nNextBlock=-1;
	dirent=(u_fs_file_directory*)content->data;
		strcpy(dirent->fname,p);
	if(flag ==1)
		strcpy(dirent->fext,q);
	dirent->fsize=0;
	dirent->flag=flag;
	long *t=malloc(sizeof(long));	
	if(op_search_free_blk(1,t)==1)
		dirent->nStartBlock=*t;
	else
		return -errno;
	free(t);	
	op_write_blk(blk,content);
	goto exit;
}

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
int op_rm(const char *org_path,int flag)
{
	u_fs_file_directory *attr=malloc(sizeof(u_fs_file_directory));
    if(op_open(org_path,attr)==-1){
    	free(attr);
		return -ENOENT; 
    }
    if(flag==1 && attr->flag==2){
    	free(attr);
		return -EISDIR; 
    }
    else if (flag==2 && attr->flag ==1){
    	free(attr);
		return -ENOTDIR; 
    }
	u_fs_disk_block* content=malloc(sizeof(u_fs_disk_block));    

    if(flag==1){/* if delete a file */
    	long next_blk=attr->nStartBlock;
		/* set the follow block unused. */
		while(next_blk!=-1){
			op_set_blk(next_blk,0);
			op_read_blk(next_blk,content);
			next_blk=content->nNextBlock;
		}
    }
    else if( !is_empty_dir(org_path)){ /* if delete a directory not empty*/
			free(attr);
			free(content);
			return -ENOTEMPTY;
	}
		
    attr->flag=0;
    if(op_setattr(org_path,attr)==-1){
		free(attr);
		free(content);
    return -1;
	}
	
	free(attr);
	free(content);
	return 0;
}

/** 
 * This function check the directory block is emtpy or not
 * 
 * @param blk indicate the directory block
 * @return 1 if empty else 0 
 */
int is_empty_dir(const char* path){
	
	u_fs_disk_block *content=malloc(sizeof(u_fs_disk_block));
	u_fs_file_directory *attr=malloc(sizeof(u_fs_file_directory));
	if(op_open(path,attr)==-1){
		goto noEmpty;
	}
    long start_blk;
    start_blk=attr->nStartBlock;
    if(attr->flag==1)
    	goto noEmpty;    	
    if(op_read_blk(start_blk, content))
		goto noEmpty;
		
       	
	u_fs_file_directory *dirent=(u_fs_file_directory*)content->data;
	int position=0;
		
	while( position<content->size ){/* the file pointer is not out of the content of direcotry */
		
        if (dirent->flag !=0 ) /* the dirent is still being used.*/
            goto noEmpty;
		/* read next directory entry */
		dirent++;
		position+=sizeof(u_fs_file_directory);
	}
	
	free(attr);
	free(content);
	return 1;
noEmpty:
    free(attr);
    free(content);
    return 0;

}

/** 
 * This function read a whole block at a time.
 * 
 * @param blk indicate the block to be read
 * @param content store the data of the block 
 *
 * @return 0 on success else -1 
 */
int op_read_blk(long blk,u_fs_disk_block * content){
	
	FILE* fp;
	fp=fopen(DISK, "r+");
	if(fp==NULL)
		return -1;

	if(fseek(fp, blk*BLOCK_BYTES, SEEK_SET)!=0)
		goto err;
	fread(content, sizeof(u_fs_disk_block), 1, fp);
	if(ferror (fp) || feof(fp))
		goto err;

	fclose(fp);
	return 0;
	
err:
	fclose(fp);
//	fprintf(stderr,"op_read_blk error\n");
	return -1;
}

/** 
 * This function write a whole block at a time.
 * 
 * @param blk indicate the block to be written
 * @param content store the block data to be written
 *
 * @return 0 on success else -1 
 */
int op_write_blk(long blk,u_fs_disk_block * content);
int op_write_blk(long blk,u_fs_disk_block * content){
	FILE* fp;
	fp=fopen(DISK, "r+");
	if(fp==NULL)
		return -1;
	if(fseek(fp, blk*BLOCK_BYTES, SEEK_SET)!=0)
		goto err;
	fwrite(content, sizeof(u_fs_disk_block), 1, fp);
	if(ferror (fp) || feof(fp))
		goto err;

	fclose(fp);
	return 0;
	
err:
	fclose(fp);
	return -1;
} 
  
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
int op_search_free_blk(int num,long* start_blk){	

    *start_blk=1+MAX_BITMAP_IN_BLOCK+1;
    int temp=0;
	FILE* fp=NULL;
	fp=fopen(DISK, "r+");
	if(fp==NULL)
		return 0;
	int start,left;
	unsigned int mask,f; 
    int *flag;
	
	int max=0;
	long max_start=-1;
	  
	while(*start_blk< max_filesystem_in_block-1){
			
		start = *start_blk/8;
		left = *start_blk%8;
		mask=1;
		mask<<=left;	
		fseek(fp,BLOCK_BYTES+start,SEEK_SET);
		flag=malloc(sizeof(int));
		fread(flag,sizeof(int),1,fp);
		f=*flag;
		for(temp=0; temp<num; temp++){
			if( (f&mask) == mask)
				break;
			if( (mask &0x80000000 )== 0x80000000){//already test all the bits of flag,read next flag
				fread(flag,sizeof(int),1,fp);
				f=*flag;
				mask=1;
			}
			else
				mask<<=1;
		}
		if(temp>max){
			max=temp;
			max_start=*start_blk;
		}
		if(temp==num)
			break;
		
		*start_blk=(temp+1)+*start_blk;
		temp=0;

	}
	*start_blk=max_start;
	fclose(fp);	
	int j=max_start;
	int i;
	for(i=0;i<max;++i){
		if(op_set_blk(j++,1)==-1)
			return -1;
	}
	
	return max;
	   
}


/**
 * The function set the block used or unused.
 * 
 * @param start_blk is the block to be set
 * @param flag indicate used or unused 
 *
 * @return 0 if success else -1 
 */
int op_set_blk(long blk,int flag){

	if(blk==-1)
		return -1;
	FILE* fp=NULL;
	fp=fopen(DISK, "r+");
	if(fp==NULL)
		return -1;
	int start=blk/8;
	int left=blk%8;
	int f;
	int mask=1;
	mask<<= left;	
	fseek(fp,BLOCK_BYTES+start,SEEK_SET);
	int *temp=malloc(sizeof(int));
	fread(temp,sizeof(int),1,fp);
	f=*temp;
	if(flag) //set the bit
		f|= mask;
	else     //reset the bit
		f&= ~mask;
	
	*temp=f;	
	fseek(fp,BLOCK_BYTES+start,SEEK_SET);
	fwrite(temp,sizeof(int),1,fp);	
	fclose(fp);
	
	return 0;
}
