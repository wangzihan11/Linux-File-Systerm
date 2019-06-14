#ifndef OP_H
#define OP_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ufs.h"

extern int find_parent(char *path, char **parent, char **fname);

extern int init_new_blk(long blk);

extern int is_exist(const char *fname, struct u_fs_data_block **content);

extern int div_string(char *path, char **surplus, char **fname);

extern int find_dirent(char *fname, struct u_fs_data_block *content, struct u_fs_inode **dirent);

extern int op_create(const char *path, int flag);

extern int op_open(const char *path, struct u_fs_inode *inode);

extern int op_modify_attr(const char *path, struct u_fs_inode *inode);

extern int op_rm(const char *path, int flag);

extern int is_empty_dir(const char *path);

extern int op_read_blk(long blk, struct u_fs_data_block *content);
 
extern int op_write_blk(long blk, struct u_fs_data_block *content);

extern int op_search_free_blk(int num, long *start_blk);

extern int op_set_blk(long blk, int flag);

#endif