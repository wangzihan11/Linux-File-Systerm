#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <memory.h>

#include "ufs.h"
#include "op.h"

static int u_fs_getattr(const char *path, struct stat *stbuf)
{
	struct u_fs_inode inode;
	memset(stbuf, 0, sizeof(struct stat));
	if(op_open(path, &inode) != 0) {
		return -ENOENT; 
	}

	if (inode.flag==2) {
		stbuf->st_mode = S_IFDIR | 0666;
	} else if (inode.flag==1) {
		stbuf->st_mode = S_IFREG | 0666;
		stbuf->st_size = inode.fsize;
	}

	return 0;
}

static int u_fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
	struct u_fs_data_block *content;
	struct u_fs_inode *dirent, *inode;
	int position = 0, res = 0;
	char name[MAX_FILENAME + 1];	
	long start_blk;
	
	content = malloc(sizeof(struct u_fs_data_block));
	inode = malloc(sizeof(struct u_fs_inode));	

	if (!content || !inode) {
		printf("Malloc failed!\n");
		res = -ENOMEM;
		goto out;
	}
	
	if (op_open(path, inode) != 0) {
		res = -ENOENT;
		goto out;
	}
	
	if(inode->flag == 1) {
		res = -ENOTDIR;
		goto out;	
	}
	start_blk = inode->nStartBlock;
	if (op_read_blk(start_blk, content) == -1) {
		printf("op_read_blk failed!\n");
		res = -1;
		goto out;
	}
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	
	dirent = (struct u_fs_inode*)content->data;
	
	while (1) {
		position = 0;
		while( position < MAX_DATA_IN_BLOCK ) {
			strcpy(name, dirent->fname);	
			if ((dirent->flag == 1 || dirent->flag == 2) && filler(buf, name, NULL, 0)) {
				break;
			}
			dirent++;
			position += sizeof(struct u_fs_inode);
		}
		if (content->nNextBlock != -1) {
			if (op_read_blk(content->nNextBlock, content) == -1) {
				printf("op_read_blk failed!\n");
				res = -1;
				goto out;
			}
			dirent = (struct u_fs_inode*)content->data;
		} else {
			break;
		}
	}
	
out:
	if (inode) {
		free(inode);
	}
	if (content) {
		free(content);
	}
	return res;
}

static int u_fs_open(const char *path, struct fuse_file_info *fi)
{
	struct u_fs_inode inode;
	if (op_open(path, &inode) != 0) {
		return -ENOENT;
	}
	return 0;
}

static int u_fs_mkdir(const char *path, mode_t mode)
{
	int res = op_create(path, 2);
	return res;
}

static int u_fs_rmdir(const char *path)
{
	int res = op_rm(path, 2);
	return res;
}

static int u_fs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res = op_create(path, 1);
	return res;
}

static int u_fs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
	int temp, res = 0;
	char *pt;
	int real_offset, blk_num, i, ret = 0;
	long start_blk;
	struct u_fs_inode *inode;
	struct u_fs_data_block *content;
	
	inode = malloc(sizeof(struct u_fs_inode));
	content = malloc(sizeof(struct u_fs_data_block)); 
	
	if (!content || !inode) {
		printf("Malloc failed!\n");
		res = -ENOMEM;
		goto out;
	}
	
	if(op_open(path,inode) != 0) {
		res = -ENOENT;
		goto out;
	}
	if(inode->flag == 2 ) {
		res = -EISDIR;
		goto out;
	}

	start_blk = inode->nStartBlock;
	if (inode->fsize <= offset) {
		size = 0;
	} else {
		if (offset + size > inode->fsize) {
			size = inode->fsize - offset;
		}
	
		blk_num = offset / MAX_DATA_IN_BLOCK;
		real_offset = offset % MAX_DATA_IN_BLOCK;
		for (i = 0; i < blk_num; i++ ) {
			if (op_read_blk(start_blk, content) == -1) {
				printf("op_read_blk failed!\n");
				res = -1;
				goto out;
			}
			start_blk = content->nNextBlock;
		}
		if (op_read_blk(start_blk, content) == -1) {
			printf("op_read_blk failed!\n");
			res = -1;
			goto out;
		}
		temp = size;
		pt = content->data;
		pt += real_offset;
		ret = (MAX_DATA_IN_BLOCK - real_offset < size ? MAX_DATA_IN_BLOCK - real_offset : size);
		memcpy(buf, pt, ret);
		temp -= ret;

		while (temp > 0) {
			if (op_read_blk(content->nNextBlock, content) == -1) {
				printf("op_read_blk failed!\n");
				res = -1;
				goto out;
			}
			if (temp > MAX_DATA_IN_BLOCK) {
				memcpy(buf + size - temp, content->data, MAX_DATA_IN_BLOCK);
				temp -= MAX_DATA_IN_BLOCK;
			} else {
				memcpy(buf + size - temp, content->data, temp);
				break;
			}
		}
	}
	res = size;
out:
	if (inode) {
		free(inode);
	}
	if (content) {
		free(content);
	}
	return res;
}

int find_offset_to_write(struct u_fs_inode *inode, off_t *offset, long *start_blk, struct u_fs_data_block **content)
{
	long blk_num, need_num, real_num, s_num, i, j;
	*start_blk = inode->nStartBlock;
	if (*offset <= inode->fsize) {
		blk_num = (*offset) / MAX_DATA_IN_BLOCK;
		*offset = (*offset) % MAX_DATA_IN_BLOCK;
		for (i = 0; i < blk_num; i++ ) {
			if (op_read_blk(*start_blk, *content) == -1) {
				printf("op_read_blk failed!\n");
				return -1;
			}
			*start_blk = (*content)->nNextBlock;
		}
		if (op_read_blk(*start_blk, *content) == -1) {
			printf("op_read_blk failed!\n");
			return -1;
		}
	} else {
		blk_num = inode->fsize / MAX_DATA_IN_BLOCK;
		for (i = 0; i < blk_num; i++ ) {
			if (op_read_blk(*start_blk, *content) == -1) {
				printf("op_read_blk failed!\n");
				return -1;
			}
			*start_blk = (*content)->nNextBlock;
		}
		if (op_read_blk(*start_blk, *content) == -1) {
			printf("op_read_blk failed!\n");
			return -1;
		}
		*offset -= (blk_num + 1) * MAX_DATA_IN_BLOCK;
		need_num = (*offset - inode->fsize) / MAX_DATA_IN_BLOCK + 1;
		real_num = op_search_free_blk(need_num, &s_num);
		if (real_num < 1) {
			printf("op_search_free_blk failed!\n");
			return -1;
		}
		while (1) {
			for (j = 0; j < real_num; j++) {
				(*content)->nNextBlock = s_num;
				(*content)->size = MAX_DATA_IN_BLOCK;
				if (op_write_blk(*start_blk, *content) == -1) {
					printf("op_write_blk failed!\n");
					return -1;
				}
				if (op_read_blk((*content)->nNextBlock, *content) == -1) {
					printf("op_read_blk failed!\n");
					return -1;
				}
				*start_blk = s_num;
				s_num = s_num + 1;
				*offset -= MAX_DATA_IN_BLOCK;
			}
			need_num -= real_num;
			if (need_num == 0) {	
				break;
			} else {
				real_num = op_search_free_blk(need_num, &s_num);
				if (real_num < 1) {
					printf("op_search_free_blk failed!\n");
					return -1;
				}
			}	
		}
	}
	return 0;
}

static int u_fs_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
	long start_blk, next_blk, next_b;
	int org_offset = offset;
	int ret = 0, total = 0;
	int num, i, res = 0;
	char *pt;
	struct u_fs_inode *inode;
	struct u_fs_data_block *content;
	
	inode = malloc( sizeof(struct u_fs_inode));
	content = malloc( sizeof(struct u_fs_data_block));
	
	if (!content || !inode) {
		printf("Malloc failed!\n");
		res = -ENOMEM;
		goto out;
	}
	
	if (op_open(path, inode) != 0) {
		res = -ENOENT;
		goto out;
	}

	if (find_offset_to_write(inode, &offset, &start_blk, &content) == -1) {
		printf("find_offset_to_write failed!\n");
		res = -1;
		goto out;
	}
	
	pt = content->data;	
	pt += offset;		
	ret = (MAX_DATA_IN_BLOCK - offset < size ? MAX_DATA_IN_BLOCK - offset : size); 		 	
	memcpy(pt, buf, ret);
	buf += ret;
	content->size += ret;
	total += ret;
	size -= ret;
	
	if(size > 0) {
 		num = op_search_free_blk(size/MAX_DATA_IN_BLOCK + 1, &next_b);
		if (num < 1) {
			printf("op_search_free_blk failed!\n");
			res = -1;
			goto out;
		}
		content->nNextBlock = next_b;
		if (op_write_blk(start_blk, content) == -1) {
			printf("op_write_blk failed!\n");
			res = -1;
			goto out;
		}
		while(1) {
			for(i = 0; i < num; ++i) {
				ret = (MAX_DATA_IN_BLOCK < size ? MAX_DATA_IN_BLOCK : size);
				content->size = ret;
				memcpy(content->data, buf, ret);
				buf += ret;
				size -= ret;
				total += ret;
				if(size == 0) {
					content->nNextBlock = -1;
				} else {
					content->nNextBlock = next_b + 1;
				}
				if (op_write_blk(next_b, content) == -1) {
					printf("op_write_blk failed!\n");
					res = -1;
					goto out;
				}
				next_b = next_b + 1;
			}
			if(size == 0) {
				break;
			}
			num = op_search_free_blk( size/MAX_DATA_IN_BLOCK + 1, &next_b);
			if (num < 1) {
				printf("op_search_free_blk failed!\n");
				res = -1;
				goto out;
			}
		}		
	} else {
		next_blk = content->nNextBlock;
		content->nNextBlock = -1;
		if (op_write_blk(start_blk, content) == -1) {
			printf("op_write_blk failed!\n");
			res = -1;
			goto out;
		}
		while(next_blk != -1) {
			if (op_set_blk(next_blk, 0) == -1) {
				printf("op_set_blk failed!\n");
				res = -1;
				goto out;
			}
			if (op_read_blk(next_blk, content) == -1) {
				printf("op_read_blk failed!\n");
				res = -1;
				goto out;
			}
			next_blk = content->nNextBlock;
		}
	}
	size = total;
	if (inode->fsize < org_offset + size) {
		inode->fsize = org_offset + size;
		if (op_modify_attr(path, inode) != 0) {
			printf("op_modify_attr failed!\n");
			res = -1;
			goto out;
		}
	}
	res = size;
out:	
	if (inode) {
		free(inode);
	}
	if (content) {
		free(content);
	}
	return res;
}

static int u_fs_unlink(const char *path)
{
	int res = op_rm(path,1);
	return res;
}

void *u_fs_init (struct fuse_conn_info *conn)
{
	FILE * fp = NULL;
	struct super_block *super_block_record;
	
	super_block_record = malloc(sizeof(struct super_block));
	
	if (!super_block_record) {
		printf("Malloc failed!\n");
		return 0;
	}
	
	fp = fopen(DISK, "r+");
	if(fp == NULL) {
		fprintf(stderr, "unsuccessful!\n");
		free(super_block_record);
		return 0;
	}
	/*  read the supper block and initilize the global variable*/
	fread(super_block_record, sizeof(struct super_block), 1, fp);
	max_filesystem_in_block = super_block_record->fs_size;
	fclose(fp);
	free(super_block_record);
	return (long*)max_filesystem_in_block;
}

static struct fuse_operations u_fs_oper = {
	.getattr	= u_fs_getattr,
	.readdir	= u_fs_readdir,
	.mkdir		= u_fs_mkdir,
	.rmdir		= u_fs_rmdir,
	.mknod		= u_fs_mknod,
	.read		= u_fs_read,
	.write		= u_fs_write,
	.unlink		= u_fs_unlink,    
	.open		= u_fs_open,
	.init		= u_fs_init,
};

int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &u_fs_oper, NULL);
}