/*
* UFS: our own Filesystem in Userspace 
* Copyright (c) 2009 LuQianhui <qianhui_lu@qq.com>
* All rights reserved.
*
* 文件名称：init.c
* 摘    要：   this is  a format program to init the image       
*          file ,  to write its super block and bitmap          
*          and  blocks data. 
* 
*
* 当前版本：1.0
* 作    者：飘零青丝
* 完成日期：2009年2月20日
*
*/

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include "ufs.h"

int main(void){
	
	FILE * fp=NULL;

	fp=fopen(DISK, "r+");
	if(fp == NULL) {
		fprintf(stderr,"open diskimg unsuccessful!\n");
		return 0;
	}
	
	sb *super_block_record=malloc(sizeof(sb));

/* calculate the size of the filesystem in block */
	fseek(fp, 0, SEEK_END);	
	super_block_record->fs_size = ftell(fp)/BLOCK_BYTES;	
	super_block_record->first_blk = 1 + MAX_BITMAP_IN_BLOCK;
	super_block_record->bitmap = MAX_BITMAP_IN_BLOCK;
		
/* initialize the super block, super block is block 0.	*/
	if(fseek(fp, 0, SEEK_SET )!=0)
		fprintf(stderr,"unsuccessful!\n");	
	fwrite(super_block_record, sizeof(sb), 1, fp);
	if(fseek(fp, 512, SEEK_SET )!=0)
		fprintf(stderr,"unsuccessful!\n");	

/*	initialize the bitmap block	*/
	/* it is the first bitmap block */
	char a[180];
	memset(a,-1,180);	
	fwrite(a, 180, 1, fp);
	int temp=0x80000000;
	int* pt=&temp;
	fwrite(pt, sizeof(int), 1, fp);
	char b[328];
	memset(b,0,328);
	fwrite(b,328,1,fp);
	/* the rest bitmap blocks*/
	int total = (MAX_BITMAP_IN_BLOCK-1)*BLOCK_BYTES;
	char rest[total];
	memset(rest, 0, total);
	fwrite(rest, total, 1, fp);

/*  initialize the root directory block */
	fseek(fp, BLOCK_BYTES * (MAX_BITMAP_IN_BLOCK+1), SEEK_SET);
    u_fs_disk_block *root=malloc(sizeof(u_fs_disk_block));
    root->size= 0;
    root->nNextBlock=-1;
    root->data[0]='\0';
    fwrite(root, sizeof(u_fs_disk_block), 1, fp);
    			
	fclose(fp);
	printf("initialize successful!\n");
	return 0;
}

