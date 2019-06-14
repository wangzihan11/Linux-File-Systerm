#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <errno.h>

#include "ufs.h"

int main(void) 
{	
	FILE *fp = NULL;
	int total = (MAX_BITMAP_BLOCK_NUM - 1) * BLOCK_BYTES;
	char a = 0xFF;
	char *c = &a;
	char b[511];
	char rest[total];
	struct super_block *super_block_record;
	struct u_fs_data_block *root;
	int err = 0;
	
	super_block_record = malloc(sizeof(struct super_block));
	root = malloc(sizeof(struct u_fs_data_block));
	
	if (!super_block_record || !root) {
		printf("Malloc failed!\n");
		err = -ENOMEM;
		goto out;
	}
	
	fp = fopen(DISK, "r+");
	if (fp == NULL) {
		fprintf(stderr, "open diskimg unsuccessful!\n");
		err = -1;
		goto out;
	}

	if (fseek(fp, 0, SEEK_END) != 0) {
		err = -1;
		goto out;
	}
	super_block_record->fs_size = ftell(fp)/BLOCK_BYTES; 
	super_block_record->first_blk = 1 + MAX_BITMAP_BLOCK_NUM;
	super_block_record->bitmap = MAX_BITMAP_BLOCK_NUM;
	
	if (fseek(fp, 0, SEEK_SET) != 0) {
		err = -1;
		goto out;
	}
	fwrite(super_block_record, sizeof(struct super_block), 1, fp);
	if (fseek(fp, 512, SEEK_SET) != 0) {
		err = -1;
		goto out;
	}

	fwrite(c, sizeof(char), 1, fp);
	memset(b, 0, 511);
	fwrite(b, 511, 1, fp);

	memset(rest, 0, total);
	fwrite(rest, total, 1, fp);

	if (fseek(fp, BLOCK_BYTES * (MAX_BITMAP_BLOCK_NUM + 1), SEEK_SET) != 0) {
		err = -1;
		goto out;
	}
	root->size = 0;
	root->nNextBlock = -1;
	root->data[0] = '\0';
	fwrite(root, sizeof(struct u_fs_data_block), 1, fp);
	
	fclose(fp);
	printf("initialize successful!\n");
	
out:
	if (super_block_record) {
		free(super_block_record);
	}
	if (root) {
		free(root);
	}
	return err;
}