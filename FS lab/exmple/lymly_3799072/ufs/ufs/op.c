#include <fcntl.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "ufs.h"
#include "op.h"

int find_parent(char *path, char **parent, char **fname)
{
	char *q_temp;
	char *ptr, *c = "/";
	q_temp = path;
	q_temp++;
	ptr = strrchr(q_temp, '/');
	if (ptr) {
		*ptr = '\0';
		ptr++;
		*fname = ptr;
		*parent = path;
	} else {
		*fname = q_temp;
		*parent = c;
	}
	return 0;
}

int init_new_blk(long blk)
{
	int err = 0;
	struct u_fs_data_block *content;
	content = malloc(sizeof(struct u_fs_data_block));
	if (!content) {
		printf("Malloc failed!\n");
		return -1;
	}
	content->size = 0;
	content->nNextBlock = -1;
	strcpy(content->data, "\0");
	if (op_write_blk(blk, content) == -1) {
		printf("op_write_blk failed\n");
		err = -1;
	}
	free(content);
	return err;
}

int is_exist(const char *fname, struct u_fs_data_block **content)
{
	struct u_fs_inode *dirent;
	int pos = 0;
	dirent = (struct u_fs_inode *)(*content)->data;
	while (1) {
		pos = 0;
		while (dirent->flag != 0 && pos < MAX_DATA_IN_BLOCK) {
			if (strcmp(fname, dirent->fname) == 0) {
				return 1;
			}
			dirent++;
			pos += sizeof(struct u_fs_inode);
		}
		if ((*content)->nNextBlock != -1) {
			if (op_read_blk((*content)->nNextBlock, *content) == -1) {
				printf("op_read_blk failed! \n");
				return -1;
			}
			dirent = (struct u_fs_inode *)(*content)->data;
		} else {
			return 0;
		}
	}
}

int op_modify_attr(const char *path, struct u_fs_inode *inode)
{
	int position = 0;
	char *fname, *parent, *path_temp;
	long start_blk;
	struct u_fs_inode *dirent, *f_inode;
	struct u_fs_data_block *content;
	int res = 0;
	int test_flag = 0;
	
	content = malloc(sizeof(struct u_fs_data_block));
	f_inode = malloc(sizeof(struct u_fs_inode));
	
	path_temp = strdup(path);
	
	if (!content || !f_inode) {
		printf("Malloc failed! \n");
		res = -ENOMEM;
		goto out;
	}
	
	find_parent(path_temp, &parent, &fname);
	if (op_open(parent, f_inode) != 0) {
		res = -ENOENT;
		goto out;
	}
	start_blk = f_inode->nStartBlock;
	if (op_read_blk(start_blk, content) == -1) {
		printf("op_read_blk failed! \n");
		res = -1;
		goto out;
	}

	while (1) {
		position = 0;
		dirent = (struct u_fs_inode *)content->data;
		while (position < MAX_DATA_IN_BLOCK) {
			if (dirent->flag != 0 && strcmp(fname, dirent->fname) == 0) { 
				dirent->fsize = inode->fsize;
				dirent->nStartBlock = inode->nStartBlock;
				dirent->flag = inode->flag;
				test_flag = 1;
				break;
			}
			dirent++; 
			position += sizeof(struct u_fs_inode);
		}
		
		if (test_flag == 0 && content->nNextBlock != -1) {
			start_blk = content->nNextBlock;
			if (op_read_blk(start_blk, content) == -1) {
				printf("op_read_blk failed! \n");
				res = -1;
				goto out;
			}
		} else {
			break;
		}
	}
	
	if (test_flag == 0) {
		res = -ENOENT;
		goto out;
	}

	if (op_write_blk(start_blk, content) == -1) {
		printf("op_write_blk failed! \n");
		res = -1;
		goto out;
	}

out:
	if (content) {
		free(content);
	}
	if (f_inode) {
		free(f_inode);
	}
	if (path_temp) {
		free(path_temp);
	}
	return res;
}

int div_string(char *path, char **surplus, char **fname)
{
	char *q_temp;
	char *ptr;
	q_temp = path;
	if (*q_temp == '/') {
		q_temp++;
	}
	ptr = strchr(q_temp, '/');
	if (ptr) {
		*ptr = '\0';
		ptr++;
		*surplus = ptr;
		*fname = q_temp;
	} else {
		*fname = q_temp;
		*surplus = NULL;
	}
	return 0;
}

int find_dirent(char *fname, struct u_fs_data_block *content, struct u_fs_inode **dirent)
{
	int pos;
	*dirent = (struct u_fs_inode *)content->data;
	while (1) {
		pos = 0;
		while (pos < MAX_DATA_IN_BLOCK) {
			if (((*dirent)->flag == 1 || (*dirent)->flag == 2) && strcmp((*dirent)->fname, fname) == 0) {
				return 0;
			}
			(*dirent)++;
			pos += sizeof(struct u_fs_inode);
		}
		if (content->nNextBlock != -1) {
			if (op_read_blk(content->nNextBlock, content) == -1) {
				printf("op_read_blk failed! \n");
				return -1;
			}
			*dirent = (struct u_fs_inode *)content->data;
		} else {
			return -1;
		}
	}
}

int op_open(const char *path, struct u_fs_inode *inode)
{
	char *subpath, *fname, *path_temp, *temp;
	long start_blk;
	struct super_block *super_block_record;
	struct u_fs_inode *dirent; 
	struct u_fs_data_block *content;
	int res = 0;
	
	content = malloc(sizeof(struct u_fs_data_block));
	
	path_temp = strdup(path);
	
	if (!content) {
		printf("Malloc failed!\n");
		res = -ENOMEM;
		goto out;
	}
	
	if (op_read_blk(0, content) == -1) {
		printf("op_read_blk failed! \n");
		res = -1;
		goto out;
	}
	super_block_record = (struct super_block *)content;
	start_blk = super_block_record->first_blk;
	
	if (strcmp(path, "/") == 0) {
		inode->flag = 2;
		inode->nStartBlock = start_blk;
		goto out;
	}
	
	if (op_read_blk(start_blk, content) == -1) {
		printf("op_read_blk failed! \n");
		res = -1;
		goto out;
	}	
	temp = path_temp;
	while (1) {
		div_string(temp, &subpath, &fname);
		if (find_dirent(fname, content, &dirent) == -1) {
			printf("find_dirent failed! \n");
			res = -1;
			goto out;
		}	
		if (subpath == NULL) {
			strcpy(inode->fname, dirent->fname);
			inode->fsize = dirent->fsize;
			inode->nStartBlock = dirent->nStartBlock;
			inode->flag = dirent->flag;
			break;
		} else {
			if (op_read_blk(dirent->nStartBlock, content) == -1) {
				printf("op_read_blk failed! \n");
				res = -1;
				goto out;
			}
		}
		temp = subpath;
	} 

out:
	if (content) {
		free(content);
	}
	if (path_temp) {
		free(path_temp);
	}
	return res;
}

int op_create(const char *path, int flag) 
{
	long p_dir, new_blk;
	char *fname, *parent, *path_temp;
	int offset = 0, res = 0;
	long temp, t;
	struct u_fs_inode *dirent, *inode;
	struct u_fs_data_block *content, *new_block; 
	
	inode = malloc(sizeof(struct u_fs_inode));
	content = malloc(sizeof(struct u_fs_data_block));
	new_block = malloc(sizeof(struct u_fs_data_block));
	path_temp = strdup(path);

	if (!inode || !content ||!new_block) {
		printf("Malloc failed! \n");
		res = -ENOMEM;
		goto out;
	}
	
	find_parent(path_temp, &parent, &fname);
	if (strlen(fname) > MAX_FILENAME) {
		res = -ENAMETOOLONG;
		goto out;
	}

	if (op_open(parent, inode) != 0) {
		res = -ENOENT;
		goto out;
	}
	p_dir = inode->nStartBlock;
	if (op_read_blk(p_dir, content) == -1) {
		printf("op_read_blk failed! \n");
		res = -1;
		goto out;
	}
	
	if (is_exist(fname, &content)) {
		res = -EEXIST;
		goto out;
	}
	
	while (1) {
		dirent = (struct u_fs_inode*)content->data;
		offset = 0;
		while (offset < MAX_DATA_IN_BLOCK) {
			if (dirent->flag == 0) {
				if (offset + sizeof(struct u_fs_inode) < MAX_DATA_IN_BLOCK) {
					goto find;
				} else {
					break;
				}
			}
			dirent++;
			offset += sizeof(struct u_fs_inode);
		}
		
		if (content->nNextBlock != -1) {
			p_dir = content->nNextBlock;
			if (op_read_blk(p_dir, content) == -1) {
				printf("op_read_blk failed! \n");
				res = -1;
				goto out;
			}
		} else {
			goto new;
		}	
	}
	
find:
	strcpy(dirent->fname, fname);
	dirent->fsize = 0;
	dirent->flag = flag;
	if (op_search_free_blk(1, &temp) < 1) {
		printf("op_search_free_blk failed! \n");
		res = -1;
		goto out;
	}
	dirent->nStartBlock = temp;
	if (op_write_blk(p_dir, content) == -1) {
		printf("op_write_blk failed! \n");
		res = -1;
		goto out;
	}
	if (init_new_blk(dirent->nStartBlock) == -1) {
		printf("init_new_blk failed! \n");
		res = -1;
		goto out;
	}
	goto out;
	
new:
	if (op_search_free_blk(1, &temp) < 1) {
		printf("op_search_free_blk failed! \n");
		res = -1;
		goto out;
	}
	new_blk = temp;
	content->nNextBlock = new_blk;
	if (op_write_blk(p_dir, content) == -1) {
		printf("op_write_blk failed! \n");
		res = -1;
		goto out;
	}
	
	new_block->nNextBlock = -1;
	dirent = (struct u_fs_inode*)new_block->data;
	strcpy(dirent->fname, fname);
	dirent->fsize = 0;
	dirent->flag = flag;
	if (op_search_free_blk(1, &t) < 1) {
		printf("op_search_free_blk failed! \n");
		res = -1;
		goto out;
	}
	dirent->nStartBlock = t;
	if (op_write_blk(new_blk, new_block) == -1) {
		printf("op_write_blk failed! \n");
		res = -1;
		goto out;
	}
	if (init_new_blk(dirent->nStartBlock) == -1) {
		printf("init_new_blk failed! \n");
		res = -1;
		goto out;
	}

out:
	if (path_temp) {
		free(path_temp);
	}
	if (inode) {
		free(inode);
	}
	if (content) {
		free(content);
	}
	if (new_block) {
		free(new_block);
	}
	return res;
}

int op_rm(const char *path, int flag)
{
	long next_blk;
	int res = 0;
	struct u_fs_inode *inode;
	struct u_fs_data_block *content;
	
	inode = malloc(sizeof(struct u_fs_inode));
	content = malloc(sizeof(struct u_fs_data_block));
	
	if (!inode || !content) {
		printf("Malloc failed!\n");
		res = -ENOMEM;
		goto out;
	}
	
	if (op_open(path, inode) != 0) {
		res = -ENOENT;
		goto out;
	}
	if (flag == 1 && inode->flag == 2) {
		res = -EISDIR;
		goto out;
	} else if (flag == 2 && inode->flag == 1) {
		res = -ENOTDIR;
		goto out;
	}
	next_blk = inode->nStartBlock;
	if (flag == 1 || (flag == 2 && is_empty_dir(path))) {
		while (next_blk != -1) {
			if (op_set_blk(next_blk, 0) == -1) {
				printf("op_set_blk failed! \n");
				res = -1;
				goto out;
			}
			if (op_read_blk(next_blk, content) == -1) {
				printf("op_read_blk failed! \n");
				res = -1;
				goto out;
			}
			if (init_new_blk(next_blk) == -1) {
				printf("init_new_blk failed! n");
				res = -1;
				goto out;
			}
			next_blk = content->nNextBlock;
		}
	} else if (!is_empty_dir(path)) {
		res = -ENOTEMPTY;
		goto out;
	}
	
	inode->flag = 0;
	inode->nStartBlock = -1;
	inode->fsize = 0;
	
	if (op_modify_attr(path, inode) != 0) {
		printf("op_modify_attr failed! \n");
		res = -1;
		goto out;
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

int is_empty_dir(const char *path)
{	
	long start_blk;
	int position = 0, res = 0;
	int test_flag = 0;
	struct u_fs_inode *dirent, *inode;
	struct u_fs_data_block *content;
	
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
	start_blk = inode->nStartBlock;
	
	if (inode->flag == 1) {
		res = -ENOTDIR;
		goto out;
	}
	
	if (op_read_blk(start_blk, content) == -1) {
		printf("op_read_blk failed! \n");
		res = -1;
		goto out;
	}
	dirent = (struct u_fs_inode*)content->data;
	
	while (1) {
		position = 0;
		while (position < MAX_DATA_IN_BLOCK) {
			if (dirent->flag == 1 || dirent->flag == 2) {
				test_flag = 1;
				goto out;
			}
			dirent++;
			position += sizeof(struct u_fs_inode);
		}
		if (content->nNextBlock != -1) {
			if (op_read_blk(content->nNextBlock, content) == -1) {
				printf("op_read_blk failed! \n");
				res = -1;
				goto out;
			}
			dirent = (struct u_fs_inode*)content->data;
		} else {
			break;
		}
	}
	
	if (test_flag == 0) {
		res = 1;
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

int op_read_blk(long blk, struct u_fs_data_block *content)
{
	FILE *fp;
	fp = fopen(DISK, "r+");
	if (fp == NULL) {
		return -1;
	}
	if (fseek(fp, blk*BLOCK_BYTES, SEEK_SET) != 0) {
		return -1;
	}
	fread(content, sizeof(struct u_fs_data_block), 1, fp);
	fclose(fp);
	return 0;
}

int op_write_blk(long blk, struct u_fs_data_block *content)
{
	FILE *fp;
	fp = fopen(DISK, "r+");
	if (fp == NULL) {
		return -1;
	}
	if (fseek(fp, blk*BLOCK_BYTES, SEEK_SET) != 0) {
		return -1;
	}
	fwrite(content, sizeof(struct u_fs_data_block), 1, fp);
	fclose(fp);
	return 0;
}

int op_search_free_blk(int num, long *start_blk)
{
	int temp = 0, max = 0;
	int start, left, i, j, *flag;
	unsigned int mask, f;
	long max_start = -1;
	*start_blk = 1 + MAX_BITMAP_BLOCK_NUM  + 1;
	FILE *fp = NULL;
	fp = fopen(DISK, "r+");
	if (fp == NULL) {
		return -1;
	}
	while (*start_blk < max_filesystem_in_block - 1) {
		start = *start_blk / 8;
		left = *start_blk % 8;
		mask = 1;
		mask <<= left;
		if (fseek(fp, BLOCK_BYTES+start, SEEK_SET) != 0) {
			return -1;
		}
		flag = malloc(sizeof(int));
		fread(flag, sizeof(int), 1, fp);
		f = *flag;
		for (temp = 0; temp < num; temp++) {
			if ((f & mask) == mask) {
				break;
			}
			if ((mask & 0x80000000) == 0x80000000) {/*read next flag*/
				fread(flag, sizeof(int), 1, fp);
				f = *flag;
				mask = 1;
			} else {
				mask <<= 1;
			}
		}
		if (temp > max) {
			max = temp;
			max_start = *start_blk;
		}
		if (temp == num) {
			break;
		}
		*start_blk = (temp + 1) + *start_blk;
		temp = 0;
	}
	*start_blk = max_start;
	fclose(fp);
	j = max_start;
	for (i = 0; i < max; ++i) {
		if (op_set_blk(j++, 1) == -1) {
			return -1;
		}
	}
	return max;
}

int op_set_blk(long blk, int flag)
{
	int start, left, f, mask;
	int temp;
	FILE *fp = NULL;
	if (blk == -1) {
		return -1;
	}
	fp = fopen(DISK, "r+");
	if (fp == NULL) {
		return -1;
	}
	start = blk / 8;
	left = blk % 8;
	mask = 1;
	mask <<= left;
	if (fseek(fp, BLOCK_BYTES + start, SEEK_SET) != 0) {
		return -1;
	}
	fread(&temp, sizeof(int), 1, fp);
	f = temp;
	if (flag) {
		f |= mask;
	} else {
		f &= ~mask;
	}
	temp = f;
	if (fseek(fp, BLOCK_BYTES+start, SEEK_SET) != 0) {
		return -1;
	}
	fwrite(&temp, sizeof(int), 1, fp);
	fclose(fp);
	return 0;
}