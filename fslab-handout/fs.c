/*
Filesystem Lab disigned and implemented by Liang Junkai,RUC
*/

#include <stdio.h>
#include<stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fuse.h>
#include <errno.h>
#include "disk.h"

#define DIRMODE S_IFDIR|0755
#define REGMODE S_IFREG|0644
#define MAX_FILENAME 8           /* 文件名的最大长度*/
#define MAX_EXTENSION 3         /* 文件扩展名的最大长度 */
#define DATABLOCK_SIZE 1024 /*4kb*/
#define MAX_FILE_NUMBER 32768
#define DerictPointer 12
#define InDerictPointer 2
#define Main_Direct_Inode_Position 4

/*大小限制*/
typedef struct 
{
	mode_t  st_mode; //文件对应的模式	4
	nlink_t st_nlink;//文件的链接数		8
	uid_t   st_uid;  //文件所有者		4
	gid_t   st_gid;  //文件所有者的组	4
	off_t   st_size; //文件字节数		8
	time_t  atime;//被访问的时间		8
	time_t  mtime;//被修改的时间		8
	time_t  ctime;//状态改变时间		8
	int direct_pointer[DerictPointer];
	int indirect_pointer[InDerictPointer];
	int id;// inode number 的编号
	char make_full[12];
	
}iNode;
typedef struct   {
	char file_name[28];
	//char file_extent[4];
	int inode_number;
}DictioryContent;
/*关于inode bitmap的寻找*/

char bit_map_mask[8] = {
						128,
						64,
						32,
						16,
						8,
						4,
						2,
						1
};
/**///用于寻找一个空的inode,并且可以看到这个inode是在第几个,
	//这里的顺序是从零开始，以后尽量也是从零开始
int find_empty_inode()
{
	int number = 0;
	char buffer[4096];
	disk_read(1, buffer);
	for (int j = 0; j < 4096; j++)
	{
		for (int k = 0; k < 8; k++)
		{
			if ((buffer[j] & bit_map_mask[k]) == 0)
				return number;
			number++;
		}
	}
	printf("we don't have more inode");
	return 0;
}
//返回的是磁盘块号和buffer中inode开始的地方
void inode_find_disk_id_and_index_in_disk(int inode_number,int * index,int * disk_id)
{
	int Inode_Data_Start_disk_id = 4;
	int disk_index = inode_number / 32;
	(*index) = (inode_number % 32);
	(*disk_id) = Inode_Data_Start_disk_id + disk_index;
	return;
}
//标记已经被占用
void make_inode_map_covered(int inode_number)
{
	char buffer[4096];
	disk_read(1, buffer);
	int bias = inode_number / 8;
	int index = inode_number % 8;
	buffer[bias] = buffer[bias] | bit_map_mask[index];
	disk_write(1, buffer);
}
char uncover_data_block[8] = { 127,191,223,239,247,251,253,254 };
void make_inode_map_uncovered(int inode_number)
{
	char buffer[4096];
	disk_read(1, buffer);
	int bias = inode_number / 8;
	int index = inode_number % 8;
	buffer[bias] = buffer[bias] | uncover_data_block [index];
	disk_write(1, buffer);
}

/**///关于data block
int find_empty_datablock()//通过通过map找到datalock的id
{
	int number = 0;
	for (int i = 2; i <= 3; i++)
	{
		char buffer[4096];
		disk_read(i, buffer);
		for (int j = 0; j < 4096; j++)
		{
			for (int k = 0; k < 8; k++)
			{
				if ((buffer[j] & bit_map_mask[k]) == 0)
					return number;
				number++;
			}
		}
	}
	 printf("we don't have more inode");
	 return 0;
}
int datablock_find_disk_id(int data_block_number)
{
	int Data_Block_start = 1028;
	return Data_Block_start + data_block_number;
}
void make_data_map_covered(int data_block_number)
{
	char buffer1[4096];
	int id;
	if (data_block_number >= 32768)
	{
		id = 3;
		data_block_number -= 32768;
	}
	else
		id = 2;
	disk_read(id, buffer1);
	int bias = data_block_number / 8;
	int index = data_block_number % 8;
	buffer1[bias] = buffer1[bias] | bit_map_mask[index];
	disk_write(id, buffer1);
}

void make_data_map_uncovered(int data_block_number)
{
	//将数据读取出来
	char buffer1[4096];
	int id;
	if (data_block_number >= 32768)
	{
		id = 3;
		data_block_number -= 32768;
	}
	else
		id = 2;
	disk_read(id, buffer1);
	int bias = data_block_number / 8;
	int index = data_block_number % 8;
	buffer1[bias] = buffer1[bias] | uncover_data_block[index];
	disk_write(id, buffer1);
}
//Format the virtual block device in the following function
/*第一个写，初始化*/
/**/ void initial_superblock()
{
	char buffer[4096] = {0};
	disk_write(0, buffer);
	return;
}
/**/void initial_inode_map()
{
	char buffer[4096] = { 0 };
	disk_write(1, buffer);
	return;
}
/**/void initial_datablock_map()
{
	char buffer[4069] = { 0 };
	disk_write(3, buffer);
	buffer[0] = 240;
	disk_write(2, buffer);
	return;
}
void give_the_value_to_inode(iNode buffer_inode[32], int index)
{
	buffer_inode[index].st_mode = DIRMODE;
	buffer_inode[index].st_nlink = 1;
	buffer_inode[index].st_uid = getuid();
	buffer_inode[index].st_gid = getgid();
	buffer_inode[index].st_size = 0;
	buffer_inode[index].atime = time(NULL);
	buffer_inode[index].mtime = time(NULL);
	buffer_inode[index].ctime = time(NULL);
	return;
}
int mkfs()
{
	initial_superblock();
	initial_inode_map();
	initial_datablock_map();
	//将主目录的编号标记成为已经使用
	make_inode_map_covered(Main_Direct_Inode_Position);
	//定位主目录所对应的inode
	int index; int disk_id;
	//按照编号找到其对应的磁盘和偏移量
	inode_find_disk_id_and_index_in_disk(Main_Direct_Inode_Position, &index, &disk_id);
	iNode buffer_inode[32];
	disk_read(disk_id, buffer_inode);
	//给主目录中的inode赋值
	give_the_value_to_inode(buffer_inode, index);
	disk_write(disk_id, buffer_inode);
	//将根目录写回去，暂时还是没用到data_block
	//开始测试--测试成功，未出现越界等情况--进行get -arr

	return 0;
}

/**/iNode just_read_inode_from_disk(int inode_number)
{
	int index; int disk_id;
	//按照编号找到其对应的磁盘和偏移量
	inode_find_disk_id_and_index_in_disk(inode_number, &index, &disk_id);
	iNode buffer_inode[32];
	disk_read(disk_id, buffer_inode);
	return buffer_inode[index];
}
/**/void repalce_iNode_according_to_disk_number(int inode_number,iNode new_file_node)
{
	int index; int disk_id;
	//按照编号找到其对应的磁盘和偏移量
	inode_find_disk_id_and_index_in_disk(inode_number, &index, &disk_id);
	iNode buffer_inode[32];
	disk_read(disk_id, buffer_inode);
	/**///进行改写
	buffer_inode[index] = new_file_node;
	//写回去续命
	disk_write(disk_id, buffer_inode);
	return;


}
/**///从目录之中寻找文件，返回文件的inode
/**//**//**/int get_min(int a, int b)
{
	if (a > b)
		return b;
	else
		return a;
}
/**//**//**/int same_name(char son_name[30], int son_name_size, DictioryContent name_we_get)
{
	/*
	//对文件名进行解析
	int extent_size = 0;
	for (int i = son_name_size - 1; i >= 0; i--)
	{
		if (son_name[i] != '.')
			extent_size++;
		else
			break;
	}
	//如果不是大小相同，证明没有扩展名
	if (extent_size == son_name_size)
		extent_size = -1;//这里是-1，是为了使得函数后面name_size本开要减去的那个.受到的影响得到消除
	int name_size = son_name_size - extent_size - 1;
	int extent_start = name_size + 1;
	//名称的比较
	int flag = 0;
	for (int i = 0; i < name_size; i++)
	{
		if (name_we_get.file_name[i] != son_name[i])
			return flag;
	}
	//防止部分匹配而有多处
	if (name_we_get.file_name[name_size] != 0)
		return flag;
	//如果名称相同，扩展名没有，那么也是ok的。
	if (extent_size == -1)
		return 1;
	//扩展名的比较
	for (int i = extent_start; i < son_name_size; i++)
	{
		if (name_we_get.file_extent[i - extent_start] != son_name[i])
			return flag;
	}
	//防止多出情况的出现
	if (name_we_get.file_extent[extent_size] != 0)
		return flag;
	return 1;*/
	for (int i = 0; i < 28; i++)
	{
		if (son_name[i] != name_we_get.file_name[i])
			return 0;
	}
	return 1;
}
/**//**//**/int visited_directly_to_get_file(int file_size, iNode father_inode, char son_name[30], int son_name_size)
{
	int disk_size = file_size / 128 + 1;
	disk_size = get_min(disk_size, DerictPointer);
	int visited_file = 0;
	for (int i = 0; i < disk_size; i++)
	{
		DictioryContent file_name_buffer[128];
		disk_read(datablock_find_disk_id(father_inode.direct_pointer[i]), file_name_buffer);
		for (int j = 0; j < 128; j++)
		{
			if (visited_file >= file_size)//特殊情况，并没有在对应的目录中找到文件
			{
				printf("		we can't find such file directly\n");
				return -ENOENT;
			}
			if (same_name(son_name, son_name_size, file_name_buffer[j]))
				return file_name_buffer[j].inode_number;
			visited_file++;
		}
	}
}
/**//**//**/int visit_first_indirect_to_get_file(int file_size, iNode father_inode, char son_name[30], int son_name_size)
{
	int visited_file = DerictPointer * 128;
	int datablock_index_buffer[1024];
	disk_read(datablock_find_disk_id(father_inode.indirect_pointer[0]), datablock_index_buffer);
	for (int i = 0; i < 1024; i++)
	{
		DictioryContent file_name_buffer[128];
		disk_read(datablock_find_disk_id(datablock_index_buffer[i]), file_name_buffer);
		for (int j = 0; j < 128; j++)
		{
			if (file_size <= visited_file)//边界条件--超出了能力范围
			{
				printf("		we can't find in first indirect \n");
				return -ENOENT;
			}
			if (same_name(son_name, son_name_size, file_name_buffer[j]))
				return file_name_buffer[i].inode_number;
			visited_file++;
		}
	}

}
/**//**//**/int visit_second_indirect_to_get_file(int file_size, iNode father_inode, char son_name[30], int son_name_size)
{
	int visited_file = DerictPointer * 128 + 1024*128;
	int datablock_index_buffer[1024];
	disk_read(datablock_find_disk_id(father_inode.indirect_pointer[1]), datablock_index_buffer);
	for (int i = 0; i < 1024; i++)
	{
		DictioryContent file_name_buffer[128];
		disk_read(datablock_find_disk_id(datablock_index_buffer[i]), file_name_buffer);
		for (int j = 0; j < 128; j++)
		{
			if (file_size <= visited_file)//边界条件--超出了能力范围
			{
				printf("		we can't find in second indirect\n");
				return -ENOENT;
			}
			if (same_name(son_name, son_name_size, file_name_buffer[j]))
				return file_name_buffer[i].inode_number;
			visited_file++;
		}
	}
}
/**//**/int find_file_in_dict(iNode father_inode,char son_name[30],int son_name_size)
{
	//对于名称进行解析
	//共有这么多的文件
	int file_size = (int)father_inode.st_size/32;
	//int disk_size = file_size / 128 + 1;//需要这么多块来存储
	//int visited_file_number = 0;
	//先进行直接访问
	int result = visited_directly_to_get_file(file_size, father_inode, son_name, son_name_size);
	/*for (int i = 0; i < get_min(disk_size, DerictPointer); i++)
	{
		DictioryContent buffer_name[128];
		disk_read(datablock_find_disk_id(father_inode.direct_pointer[i]), buffer_name);
		int count = 0;
		while ((visited_file_number < file_size)&&(count<128))
		{
			if (same_name(son_name[30], son_name_size, buffer_name[count]))
				return buffer_name[count].inode_number;
			count++;
			visited_file_number++;
		}
		if (visited_file_number == file_size)
		{
			printf("we can't find");
			return 0;
		}
	}*/
	//判断是否要进行第一次间接访问
	if ((file_size > DerictPointer * 128)&&(result== -ENOENT))
	{
		result = visit_first_indirect_to_get_file(file_size, father_inode, son_name, son_name_size);
		//判断是都要进行第二次简介访问
		if ((file_size > DerictPointer * 128 + 1024 * 128)&&(result== -ENOENT))
			result = visit_second_indirect_to_get_file(file_size, father_inode, son_name, son_name_size);
		/*disk_size -= 12;
		//这是指向数据块的编号
		int buffer_data_number[1024];
		for (int i = 0; i < 2; i++)
		{
			//指向了1024个数据库块
			disk_read(datablock_find_disk_id(father_inode.indirect_pointer[i]),buffer_data_number);
			int index_count = 0;
			while ((index_count < 1024) && (visited_file_number < file_size))
			{
				//读取一个数据块对应的所有文件名称
				DictioryContent buffer_name[128];
				disk_read(datablock_find_disk_id(buffer_data_number[index_count]), buffer_name);
				int name_count = 0;
				while ((name_count < 128) && (visited_file_number < file_size))
				{
					if (same_name(son_name[30], son_name_size, buffer_name[name_count]))
						return buffer_name[name_count].inode_number;
					name_count++;
					visited_file_number++;
				}
				index_count++;
				if (visited_file_number == file_size)
				{
					printf("we can't find");
					return 0;
				}

			}
			if (visited_file_number == file_size)
			{
				printf("we can't find");
				return 0;
			}
			}*/
	}
	if(result == -ENOENT)
		printf("		we can't find in all palces\n");
	//返回最终的结果
	return result;
}

//返回值是最终文件所在的inode的编号
/**/int understand_the_path(const char * path)
{
	int length = 0;
	while (path[length] != 0)
		length++;
	//printf("length is %d\n", length);
	printf("	understanding the file'");
	for (int i = 0; i < length; i++)
		printf("%c", path[i]);
	printf("'length is %d",length);
	printf("\n");
	int iteration = 1;
	int result_number = Main_Direct_Inode_Position;
	//得到的inode
	iNode father_inode = just_read_inode_from_disk(Main_Direct_Inode_Position);
	/*得到了根目录的文件数量*/
	while (iteration < length)
	{
		char son_name[30] = { 0 };
		int son_name_size = 0;
		while ((iteration < length) && (path[iteration] != '/'))
		{
			son_name[son_name_size] = path[iteration];
			son_name_size++;
			iteration++;
		}
		//得到一个长度为son_name_size的名字
		//判断是文件还是目录
		if (iteration == length)//是最终的文件了
		{
			result_number = find_file_in_dict(father_inode, son_name, son_name_size);
		}
		else//是目录
		{
			int time_inode_number = find_file_in_dict(father_inode, son_name, son_name_size);
			//在这里做一个简单的改动？
			father_inode = just_read_inode_from_disk(time_inode_number);
			iteration++;//这里是为了完美的跳过‘/’这个符号
		}
	}
	return result_number;

}
/**/void pass_value_from_iNode_to_stat(iNode aim_iNode, struct stat *attr)
{
	attr->st_mode = aim_iNode.st_mode;
	attr->st_nlink = aim_iNode.st_nlink;
	attr->st_uid = aim_iNode.st_uid;
	attr -> st_gid = aim_iNode.st_gid;
	attr->st_size = aim_iNode.st_size;
	attr->st_atime = aim_iNode.atime;
	attr->st_ctime = aim_iNode.ctime;
	attr->st_mtime = aim_iNode.mtime;
	return;
}
//Filesystem operations that you need to impleme
/*cd *//*第二个写*/
int fs_getattr (const char *path, struct stat *attr)
{
	//将原来stat中的数据清零
	printf("start to getatr\n");
	memset(attr, 0, sizeof(struct stat));
	//找到文件所在的iNode
	int iNode_Number = understand_the_path(path);
	if (iNode_Number == -ENOENT)
		return iNode_Number;
	//从路径中读出来
	iNode aim_inode = just_read_inode_from_disk(iNode_Number);
	printf("	the inode number we get  in get_arr is %d\n", iNode_Number);
	//进行赋值操作
	pass_value_from_iNode_to_stat(aim_inode, attr);
	printf("Getattr is called:%s\n",path);
	return 0;
}

void get_fuul_disk_according_to_rank(int rank, DictioryContent * content,iNode aim_inode)
{
	if (rank < 12)
		disk_read(datablock_find_disk_id(aim_inode.direct_pointer[rank]), content);
	else
	{
		int index = (rank - 11) / 1024;
		int trans_bias = rank - 11 - index * 1024;
		int buffer_data_block_number[1024];
		disk_read(datablock_find_disk_id(aim_inode.indirect_pointer[index]), buffer_data_block_number);
		disk_read(datablock_find_disk_id(buffer_data_block_number[trans_bias]), content);

	}
}
//已经的是过来了，但是只支持直接指向，完成度不是很高
//已经重新改写，足够支持全局的访问了（直接和间接都行）
int fs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	//解析路径
	int iNode_number = understand_the_path(path);
	printf("the read_dir number is %d\n",iNode_number );
	//读取这个目录对应的inode
	iNode aim_node = just_read_inode_from_disk(iNode_number);
	int visited_size = 0;
	int file_size = aim_node.st_size / 32;
	printf("the read_dir file_number is %d\n", file_size);
	DictioryContent tem_buffer[128];
	disk_read(datablock_find_disk_id(aim_node.direct_pointer[0]), tem_buffer);
	//printf("in reading the name is %s?????????????????????", tem_buffer[0].file_name);
	int disk_size = file_size / 128+1;
	//从inode中读取数据
	///**/直接读取
	for (int i = 0; i < disk_size; i++)
	{
		DictioryContent file_name_buffer[128];
		get_fuul_disk_according_to_rank(i, file_name_buffer,aim_node);
		int cnt = 0;
		while ((cnt < 128) && (visited_size < file_size))
		{
			filler(buffer, file_name_buffer[cnt].file_name, NULL, 0);
			visited_size++;
			cnt++;
		}
	}
	/*for (int i = 0; i < get_min(disk_size, DerictPointer); i++)
	{
		DictioryContent file_name_buffer[128];
		disk_read(datablock_find_disk_id(aim_node.direct_pointer[i]), file_name_buffer);
		for (int j = 0; j < 128; j++)
		{
			if (visited_size >= file_size)
				return 0; 
			//读一个放进去一个
			printf(" %d get file name",j);
			int k = 0;
			while (file_name_buffer[j].file_name[k]!=0)
			{
				printf("%c", file_name_buffer[j].file_name[k]);
				k++;
			}
			printf("'\n");
			filler(buffer, file_name_buffer[j].file_name, NULL, 0);
				visited_size++;
		}
	}*/
	printf("Readdir is called:%s\n", path);
	return 0;
}

/**///根据inode中datablock排第几（从第一个到第DerictPointer + 1024 *2个），将其所对应的4K的数据块读出来
/**/// 在主函数中务必要进行free操--更改了储存方式，已经不需要了
/**///返回值是一个长为4096的数组
/**/void  read_the_one_disk_according_to_rank_in_inode(int rank,iNode now_file_inode,char * content_buffer)
{
	//char  content_buffer[4096];
	if (rank < 12)
		disk_read(datablock_find_disk_id(now_file_inode.direct_pointer[rank]), content_buffer);
	else
	{
		//判断实在第几个间接
		int index = rank / (1024 + 12);
		int transdfer_rank = rank - index * (1024 ) - 12;
		int data_block_number = now_file_inode.indirect_pointer[index];
		//将间接指向的1024个data block 编号读出来
		char * real_data_node_number[1024];
		disk_read(datablock_find_disk_id(data_block_number), real_data_node_number);
		data_block_number = real_data_node_number[transdfer_rank];
		//将最终数据读出来
		disk_read(datablock_find_disk_id(data_block_number), content_buffer);

	}
	return ;

}
//目前已经写完成了，但是还没有经过测试
//等待完成了fs_write之后进行共同的测试
int fs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi)
{
	printf("important !!!!!!!!!!!!!!!!!!!!!!:  start to read\n");
	//buffer = (char*)calloc((int)size, sizeof(char));
	int buffer_char_size = 4096;//方便之后操作
	/*
	先进行第一个由于offset问题而可能不能对其的磁盘的读取
	在进行中间的读取
		读取要分是直接的
		还是第一次间接
		还是第二次间接
	最后进行最后一个磁盘的读取，最后一个磁盘的读取（可能会不是整数）
	*/
	//读取这个文件对应的inode
	int iNode_number = understand_the_path(path);
	printf("reading file : inode number is %d\n", iNode_number);
	iNode now_file_inode = just_read_inode_from_disk(iNode_number);
	//排除特殊情况
	
	/*if ((int)size + (int)offset > (int)(now_file_inode.st_size))
		return EOF;*/
	//看一下对比一下三个磁盘号码
	int sum_content_disk_amount = (int)now_file_inode.st_size / buffer_char_size+ 1;
	int start_disk_rank = offset / buffer_char_size;//从第几块磁盘开始阅读
	int start_bias = offset % buffer_char_size;//第一次阅读的偏移量是多少
	int have_read_size = 0;//已经阅读过的数量
	//开始循环阅读
	printf("in reading the size we ask is %d", size);
	while (have_read_size != (int)size)
	{
		//打开开始序号对应的磁盘的内容
		char tem_read_buffer[4096];
		read_the_one_disk_according_to_rank_in_inode(start_disk_rank, now_file_inode,tem_read_buffer);
		//逐字阅读
		for (int i = start_bias; i < buffer_char_size; i++)
		{
			//printf("in reading : we read %c\n", tem_read_buffer[i]);
			buffer[have_read_size] = tem_read_buffer[i];
			have_read_size++;
			if (have_read_size == (int)size)
				break;
		}
		//新开一块磁盘（通过顺延的方式）
		start_disk_rank++;
		//新开辟的一定要从头开始读取
		start_bias = 0;
	}

	for (int i = 0; i < 5; i++)
		printf("in reading :here is the result %c\n", buffer[i]);
	//读取过程结束
	printf("Read is called:%s\n",path);
	//free(tem_member_buff);
	printf("in reading : len is %d\n", get_min((int)size, (int)now_file_inode.st_size));
	return get_min((int)size, (int)now_file_inode.st_size);
}

/**/ int apply_a_new_inode_and_initial()
{
	//找到没有被占用的空间
	int new_inode_number = find_empty_inode();
	//标记为已经占用
	make_inode_map_covered(new_inode_number);
	//找到新申请的inode对应的磁盘并且读取出来
	int index, disk_id;
	inode_find_disk_id_and_index_in_disk(new_inode_number, &index, &disk_id);
	iNode inode_buffer[32];
	disk_read(disk_id, inode_buffer);
	//对应的inode为inode_buffer[index]
	//接下来进行写入
	inode_buffer[index].st_mode = REGMODE;
	inode_buffer[index].st_nlink = 1;
	inode_buffer[index].st_uid = getuid();
	inode_buffer[index].st_gid = getgid();
	inode_buffer[index].st_size = 0;
	inode_buffer[index].atime = time(NULL);
	inode_buffer[index].mtime = time(NULL);
	inode_buffer[index].ctime = time(NULL);
	inode_buffer[index].id = new_inode_number;
	//写入之后将刷新过后的东西写回去
	disk_write(disk_id, inode_buffer);
	return new_inode_number;
	
}
/**/ int apply_a_new_inode_and_initial_as_dict()
{
	//找到没有被占用的空间
	int new_inode_number = find_empty_inode();
	//标记为已经占用
	make_inode_map_covered(new_inode_number);
	//找到新申请的inode对应的磁盘并且读取出来
	int index, disk_id;
	inode_find_disk_id_and_index_in_disk(new_inode_number, &index, &disk_id);
	iNode inode_buffer[32];
	disk_read(disk_id, inode_buffer);
	//对应的inode为inode_buffer[index]
	//接下来进行写入
	inode_buffer[index].st_mode = DIRMODE;
	inode_buffer[index].st_nlink = 1;
	inode_buffer[index].st_uid = getuid();
	inode_buffer[index].st_gid = getgid();
	inode_buffer[index].st_size = 0;
	inode_buffer[index].atime = time(NULL);
	inode_buffer[index].mtime = time(NULL);
	inode_buffer[index].ctime = time(NULL);
	inode_buffer[index].id = new_inode_number;
	//写入之后将刷新过后的东西写回去
	disk_write(disk_id, inode_buffer);
	return new_inode_number;
}
//传入的是文件在这个datablock中的id，
/**//**/void add_a_dict_in_data_block(int data_block_node_number,DictioryContent aim_file,int file_position)
{
	
	//将datablock对应的硬盘读出来
	int disk_id = datablock_find_disk_id(data_block_node_number);
	DictioryContent file_buffer[128];
	disk_read(disk_id, file_buffer);
	//将对应的标记上写出来
	file_buffer[file_position] = aim_file;
	//写回去
	disk_write(disk_id, file_buffer);
	//进行状态标记

	return;
}
//目标是将文件挂载在目录上，现在只修改了一层，并且还没有标记状态
//并且进行状态标记
/**/ void  read_the_one_disk_according_to_rank(int rank, iNode now_file_inode, DictioryContent * content_buffer)
{
	//char  content_buffer[4096];
	if (rank < 12)
		disk_read(datablock_find_disk_id(now_file_inode.direct_pointer[rank]), content_buffer);
	else
	{
		//判断实在第几个间接
		int index = rank / (1024 + 12);
		int transdfer_rank = rank - index * (1024) - 12;
		int data_block_number = now_file_inode.indirect_pointer[index];
		//将间接指向的1024个data block 编号读出来
		char * real_data_node_number[1024];
		disk_read(datablock_find_disk_id(data_block_number), real_data_node_number);
		data_block_number = real_data_node_number[transdfer_rank];
		//将最终数据读出来
		disk_read(datablock_find_disk_id(data_block_number), content_buffer);

	}
	return;

}
/**/void write_into_disk_acccording_to_rank(int rank, iNode new_file_node, DictioryContent  *content)
{
	if (rank < 12)
		disk_write(datablock_find_disk_id(new_file_node.direct_pointer[rank]), content);
	else
	{
		//判断实在第几个间接
		int index = rank / (1024 + 12);
		int transdfer_rank = rank - index * (1024) - 12;
		int data_block_number = new_file_node.indirect_pointer[index];
		//将间接指向的1024个data block 编号读出来
		char * real_data_node_number[1024];
		disk_read(datablock_find_disk_id(data_block_number), real_data_node_number);
		data_block_number = real_data_node_number[transdfer_rank];
		//写进去
		disk_write(datablock_find_disk_id(data_block_number), content);

	}

}
/**/iNode add_file_to_dict(iNode dict_inode,DictioryContent aim_file)
{

	/*
	每个inode中都指向不同的datablock
	需要判断几种情况：直接指向还是间接（使用disk_size进行判断）
	还是在直接使用中，或是在第一次简介，或许是第二次简介
	在每一种情况中又有两种可能性，一种是需要申请新的datablock
	另一种是不需要申请新的datablock,这个可以通过file_number来进行判断
	*/
	//现在的文件数量
	/*int now_file_size = dict_inode.st_size / 32;
	int disk_amount = now_file_size / 128;
	int disk_bias = now_file_size % 128;
	//先处理要加入的情况,正好在间接的分岔口
	if ((disk_bias == 0) && (disk_amount == 12))
	{
		int datalbock_number = find_empty_datablock();
		make_data_map_covered(datalbock_number);
		dict_inode.indirect_pointer[0] = datalbock_number;
	}
	if ((disk_bias == 0) && (disk_amount == (12 + 1024)))
	{
		int datalbock_number = find_empty_datablock();
		make_data_map_covered(datalbock_number);
		dict_inode.indirect_pointer[1] = datalbock_number;
	}
	//在处理完之后,分配新的数据块,创建陈工
	if (disk_bias == 0)
	{
		if (disk_amount < 12)
		{
			int data_number = find_empty_datablock();
			make_data_map_covered(data_number);
			dict_inode.direct_pointer[disk_amount] = data_number;
			printf("in mounting process , the data_number is %d", data_number);
		}
		else
		{
			int index = (disk_amount - 12) / 1024;
			int transfer_bias = disk_amount - 12 - index * 1024;
			int buffer_number[1024];
			disk_read(datablock_find_disk_id(dict_inode.indirect_pointer[index]), buffer_number);
			int data_number = find_empty_datablock();
			make_data_map_covered(data_number);
			buffer_number[transfer_bias] = data_number;
			disk_write(datablock_find_disk_id(dict_inode.indirect_pointer[index]), buffer_number);
			
		}
	}
	//创建之后，按顺序去读取
	DictioryContent content_buffer[128];
	read_the_one_disk_according_to_rank(disk_amount, dict_inode, content_buffer);
	content_buffer[disk_bias] = aim_file;
	write_into_disk_acccording_to_rank(disk_amount, dict_inode, content_buffer);

	*/
	int pre_file_number = dict_inode.st_size / 32;
	//现在使用的磁盘数量
	int pre_disk_number = pre_file_number / 128;

	printf("		pre_file_number is %d\n", pre_file_number);
	if (pre_disk_number < 12)//证明没有全部占满
	{
		if (pre_file_number % 128 == 0)//需要申请新的空间
		{
			//申请一个新的datablock并且标记
			int data_block_node_number = find_empty_datablock();
			make_data_map_covered(data_block_node_number);
			printf("			the new_get_data_block_number is %d\n", data_block_node_number);
			//这里申请的datablock是作为储存目录下的文件信息而存在的
			dict_inode.direct_pointer[pre_file_number / 128] = data_block_node_number;
			//这里做的事情是把文件信息写入新建的数据块之中
			add_a_dict_in_data_block(data_block_node_number, aim_file, 0);
		}
		else//直接在后面加上就好
		{
			int bias = (pre_file_number % 128);
			add_a_dict_in_data_block(dict_inode.direct_pointer[pre_disk_number], aim_file,bias);
		}
	}
	else
	{
		if (pre_disk_number < 12 + 1024)
		{
			if (pre_disk_number == 12)
			{
				int  data_number = find_empty_datablock();
				make_data_map_covered(data_number);
				dict_inode.indirect_pointer[0] = data_number;

			}
			if (pre_file_number % 128 == 0)
			{
				int bias = (pre_file_number % 128);
				int buffer_number[1024];
				disk_read(datablock_find_disk_id(dict_inode.direct_pointer[0]), buffer_number);
				int data_block_node_number = find_empty_datablock();
				make_data_map_covered(data_block_node_number);
				buffer_number[pre_disk_number - 12] = data_block_node_number;
				disk_write(datablock_find_disk_id(dict_inode.direct_pointer[0]), buffer_number);
				add_a_dict_in_data_block(buffer_number[pre_disk_number - 12], aim_file, bias);
			}
			else
			{
				int bias = (pre_file_number % 128);
				int buffer_number[1024];
				disk_read(datablock_find_disk_id(dict_inode.direct_pointer[0]), buffer_number);
				add_a_dict_in_data_block(buffer_number[pre_disk_number - 12], aim_file, bias);
				
			}

		}
	}
	//在修改完成之后标记状态
	dict_inode.st_size += 32;
	dict_inode.mtime = time(NULL);
	dict_inode.ctime = time(NULL);
	return dict_inode;
	//if((pre_disk_number>=12)&&(pre_disk_number))
}
//缺少函数，没有测试


//现在是完全拷贝mkdir函数,
/*
这个版本修改了初始化的函数
从apply_a_new_inode_and_initial_as_dict()
初始化到了apply_a_new_inode_and_initial_as_dict

*/
int fs_mknod(const char *path, mode_t mode, dev_t dev)
{
	//最初路径的大小
	int length = 0;
	while (path[length] != 0)
		length++;
	//要写的文件的文件名的大小
	int last_file_name_length = 0;
	while (path[length - last_file_name_length - 1] != '/')
	{
		last_file_name_length++;
	}
	char * dict_path = (char*)calloc(length + 10, sizeof(char));
	char file_name[30] = { 0 };
	//得到文件的名称作为一个单独的字符串
	int start = length - last_file_name_length;
	for (int i = start; i < length; i++)
		file_name[i - start] = path[i];
	//得到文件路径--通过清空文件名的方式
	printf("	the dict name length is %d\n", length - last_file_name_length - 1);
	if (length - last_file_name_length - 1 == 0)
		dict_path[0] = '/';
	for (int i = 0; i < length - last_file_name_length - 1; i++)
		dict_path[i] = path[i];


	//得到了目录中对应的inode_number
	int father_inode_number = understand_the_path(dict_path);
	printf("	the dict_inode number is %d\n", father_inode_number);
	//为一个文件信息进行初始化
	DictioryContent new_file;
	for (int i = 0; i < 28; i++)
		new_file.file_name[i] = file_name[i];
	new_file.inode_number = apply_a_new_inode_and_initial();
	printf("	new inode _number is %d ！！！！！！！！！！！！！！！！！！！\n", new_file.inode_number);
	printf("	new dict name is' ");
	for (int i = 0; i < last_file_name_length; i++)
		printf("%c", new_file.file_name[i]);
	printf("'\n");


	printf("	the inode_number of dict is %d\n", father_inode_number);
	//以上没有错误

	//将文件挂载到目录上。
	/**///首先将目录的inode读取出来
	int disk_id, index;
	inode_find_disk_id_and_index_in_disk(father_inode_number, &index, &disk_id);
	iNode buffer_inode[32];
	disk_read(disk_id, buffer_inode);

	/**///在讲文件信息挂到目录里面
	printf("	start to mount\n");
	buffer_inode[index] = add_file_to_dict(buffer_inode[index], new_file);
	printf("	after mounting ,we test store data %d\n", buffer_inode[index].direct_pointer[0]);
	disk_write(disk_id, buffer_inode);
	
	/*
	开始测试对应的数据块的
	*/
	int test_id = datablock_find_disk_id(buffer_inode[index].direct_pointer[0]);
	printf("	test_id is %d\n", test_id);
	DictioryContent test_content[128];
	disk_read(test_id, test_content);
	printf("	test_after_create :%s\n", test_content[0].file_name);
	//printf("	test file inode is %d\n", test_content[0].inode_number);
	//printf("finish add\n");
	//在讲目录的inode写回去
	
	//防止内存泄漏
	free(dict_path);
	//暂时防止过多
	if (buffer_inode[index].st_size / 32 > 128 * DerictPointer)
		return -ENOSPC;
	printf("Mknod is called:%s\n",path);
	return 0;
}
/**/
int fs_mkdir (const char *path, mode_t mode)
{
	//最初路径的大小
	int length = 0;
	while (path[length] != 0)
		length++;
	//要写的文件的文件名的大小
	int last_file_name_length = 0;
	while (path[length - last_file_name_length - 1] != '/')
	{
		last_file_name_length++;
	}
	char * dict_path = (char*)calloc(length + 10, sizeof(char));
	char file_name[30] = {0};
	//得到文件的名称作为一个单独的字符串
	int start = length - last_file_name_length;
	for (int i = start; i < length; i++)
		file_name[i - start] = path[i];
	//得到文件路径--通过清空文件名的方式
	printf("	the dict name length is %d\n", length - last_file_name_length - 1);
	if (length - last_file_name_length - 1 == 0)
		dict_path[0] = '/';
	for (int i = 0; i < length - last_file_name_length - 1; i++)
		dict_path[i] = path[i];

	//得到了目录中对应的inode_number
	int father_inode_number = understand_the_path(dict_path);
	printf("	the dict_inode number is %d\n", father_inode_number);
	//为一个文件信息进行初始化
	DictioryContent new_file;
	for (int i = 0; i < 28; i++)
		new_file.file_name[i] = file_name[i];
	new_file.inode_number = apply_a_new_inode_and_initial_as_dict();
	printf("	new inode _number is %d ！！！！！！！！！！！！！！！！！！！\n", new_file.inode_number);
	printf("	new dict name is' ");
	for (int i = 0; i < last_file_name_length; i++)
		printf("%c",new_file.file_name[i]);
	printf("'\n");


	printf("	the inode_number of dict is %d\n", father_inode_number);
	//以上没有错误
	//将文件挂载到目录上。
	/**///首先将目录的inode读取出来
	int disk_id, index;
	inode_find_disk_id_and_index_in_disk(father_inode_number, &index, &disk_id);
	iNode buffer_inode[32];
	disk_read(disk_id, buffer_inode);

	/**///在讲文件信息挂到目录里面
	printf("	start to mount\n");
	buffer_inode[index] = add_file_to_dict(buffer_inode[index], new_file);
	printf("	after mounting ,we test store data %d\n", buffer_inode[index].direct_pointer[0]);

	//写回去
	disk_write(disk_id, buffer_inode);
	//测试

	int test_id = datablock_find_disk_id(buffer_inode[index].direct_pointer[0]);
	printf("	test_id is %d\n", test_id);
	DictioryContent test_content[128];
	disk_read(test_id, test_content);
	printf("	test_after_create :%s\n", test_content[0].file_name);
	//防止内存泄漏
	free(dict_path);
	//暂时防止过多
	if (buffer_inode[index].st_size / 32 > 128 * DerictPointer)
		return -ENOSPC;

	printf("Mkdir is called:%s\n",path);
	return 0;
}
/**//**/ void release_file_databock_by_provide_inode(iNode aim_inode)
{
	int datablock_amount = aim_inode.st_size / 4096 + 1;
	//先释放直接的块
	for (int i = 0; i < get_min(12, datablock_amount); i++)
		make_data_map_uncovered(aim_inode.direct_pointer[i]);
	//再释放间接的块
	if (datablock_amount > 11)
	{
		int bias = datablock_amount - 11;
		int buffer_datablcok_member[1024];
		//进行读取操作
		disk_read(datablock_find_disk_id(aim_inode.indirect_pointer[0]), buffer_datablcok_member);
		for (int i = 0; i < get_min(1024, bias); i++)
			make_data_map_uncovered(buffer_datablcok_member[i]);
	}
	//进行第二个间接的块的释放操作
	if (datablock_amount > 11 + 1024)
	{
		int bias = datablock_amount - 11 - 1024;
		int buffer_datablcok_member[1024];
		disk_read(datablock_find_disk_id(aim_inode.indirect_pointer[1]), buffer_datablcok_member);
		for (int i = 0; i < get_min(1024, bias); i++)
			make_data_map_uncovered(buffer_datablcok_member[i]);
	}
	//完成
	return;
}
/**/DictioryContent get_filename_data_from_dir_in_rank_view(int rank, iNode dir_inode)
{
	//每一个Dict的大小是32，而一个块里面是128个这样子的东西
	int disk_rank = rank / 128;//从零开始
	int disk_bias = rank % 128;//也是从零开始
	DictioryContent buffer_file_info[128];
	//读取整个块
	if (rank < 12)
		disk_read(datablock_find_disk_id(dir_inode.direct_pointer[disk_rank]), buffer_file_info);
	else
	{
		int index = (rank - 11) / 1024;//获得是第几个间接
		int indirect_bias = rank - 11 - index * 1024;//再第几个间接中的块的偏移量
		int buffer_datablock_buffer[1024];
		disk_read(datablock_find_disk_id(dir_inode.indirect_pointer[index]), buffer_datablock_buffer);
		disk_read(datablock_find_disk_id(buffer_datablock_buffer[indirect_bias]), buffer_datablock_buffer);
	}
	return buffer_file_info[disk_bias];
}

/**///暂时完成，没有测试！！！！！！！！！！！！！！！！！！！！！！！！！
/**///测试完成
int fs_rmdir (const char *path)
{
	//解析目录所在的路径
	int dir_iNode_number = understand_the_path(path);
	iNode dir_inode = just_read_inode_from_disk(dir_iNode_number);
	//解析目录所在的目录
	char * old_dir = (char *)calloc(sizeof(path), sizeof(char));
	int  old_dir_len = sizeof(path) - 1;
	while (path[old_dir_len] != '/')
		old_dir_len--;
	for (int i = 0; i < old_dir_len; i++)
		old_dir[i] = path[i];
	//对于目录中的文件进行释放
	int file_amount = dir_inode.st_size / 32;
	for (int i = 0; i < file_amount; i++)
	{
		//得到一个文件的信息
		DictioryContent file_info = get_filename_data_from_dir_in_rank_view(i, dir_inode);
		//删除文件所对应的数据块
		release_file_databock_by_provide_inode(just_read_inode_from_disk(file_info.inode_number));
		//释放inode
		make_data_map_uncovered(file_info.inode_number);
	}
	//文件释放完成
	//当作文件删除
	fs_unlink(path);
	printf("Rmdir is called:%s\n",path);
	return 0;
}


/**///模仿上面做一个按顺序写入的操作，再detelte中调用！！！！！！！！！！！！！！！！！！！！！！！！！！！！
/**/void write_filename_intodir_in_rank_view(int rank, iNode dir_inode, DictioryContent aim_data)
{
	//每一个Dict的大小是32，而一个块里面是128个这样子的东西
	int disk_rank = rank / 128;//从零开始
	int bias = rank % 128;//也是从零开始
	DictioryContent buffer_file_info[128];
	//读取整个块
	if (rank < 12)
	{
		disk_read(datablock_find_disk_id(dir_inode.direct_pointer[disk_rank]), buffer_file_info);
		printf("		formal name is %s\n",buffer_file_info[bias].file_name);
		buffer_file_info[bias] = aim_data;
		disk_write(datablock_find_disk_id(dir_inode.direct_pointer[disk_rank]), buffer_file_info);
	}
	else
	{
		int index = (rank - 11) / 1024;//获得是第几个间接
		int indirect_bias = rank - 11 - index * 1024;//再第几个间接中的块的偏移量
		int buffer_datablock_buffer[1024];
		disk_read(datablock_find_disk_id(dir_inode.indirect_pointer[index]), buffer_datablock_buffer);
		disk_read(datablock_find_disk_id(buffer_datablock_buffer[indirect_bias]), buffer_datablock_buffer);
		buffer_file_info[bias] = aim_data;
		disk_write(datablock_find_disk_id(buffer_datablock_buffer[indirect_bias]), buffer_datablock_buffer);
	}
	return;
}
/**/void delete_file_according_to_rank(int rank, iNode dir_inode)
{
	//判断是否再最后一个
	int file_amount = dir_inode.st_size / 32;
	if (rank == file_amount)
		return;
	//获得末尾的编号
	DictioryContent last_file = get_filename_data_from_dir_in_rank_view(file_amount - 1, dir_inode);
	printf("		when delete we get name %s\n",last_file.file_name);
	// 将末尾写入到原来的编号
	write_filename_intodir_in_rank_view(rank, dir_inode, last_file);
	return;
}
//释放整个数据块所对应的datablock

//第一版完成，等待；
//根目录下测试已经完成
int fs_unlink (const char *path)
{
	//进行路径解析
	printf("start to use unlink \n");
	char * dir_name = (char*)calloc(sizeof(path), sizeof(char));
	char * file_name = (char*)calloc(28, sizeof(char));
	int path_length = sizeof(path)-1;
	while (path[path_length] != '/')
		path_length--;
	for (int i = 0; i < path_length; i++)
		dir_name[i] = path[i];
	for (int i = path_length + 1; i < path_length+get_min(sizeof(path), 28); i++)
		file_name[i-path_length-1] = path[i];
	if (path_length == 0)
		dir_name[0] = '/';
	//得到了 dir_name和file_name
	//对文件进行寻址
	//对于路径解析进行测试
	printf("	dir_name is %s\n", dir_name);
	printf("	file_name is %s\n ", file_name);

	int file_inode = understand_the_path(path);
	//对于目录进行寻址
	int dir_inode_number = understand_the_path(dir_name);
	iNode dir_iNode = just_read_inode_from_disk(dir_inode_number);
	int file_amount = dir_iNode.st_size / 32;
	//先默认解析式正确的
	printf("	the dir amount is %d\n", file_amount);
	for (int i = 0; i < file_amount; i++)
	{
		printf("	in the porcess we have many names like : %s \n", get_filename_data_from_dir_in_rank_view(i, dir_iNode).file_name);
		if (get_filename_data_from_dir_in_rank_view(i, dir_iNode).inode_number == file_inode)
		{
			printf("	the same file name we get is %s\n", get_filename_data_from_dir_in_rank_view(i, dir_iNode).file_name);
			//释放数据块
			release_file_databock_by_provide_inode(just_read_inode_from_disk(file_inode));
			//删除iNode和进行移位
			/**///////错误在于移动位置的地方
			delete_file_according_to_rank(i, dir_iNode);
			break;
		}
	}

	make_inode_map_uncovered(file_inode);
	//对于父目录进行时间更新和大小
	dir_iNode.st_size -= 32;
	dir_iNode.ctime = time(NULL);
	dir_iNode.mtime = time(NULL);
	repalce_iNode_according_to_disk_number(dir_inode_number, dir_iNode);
	printf("Unlink is callded:%s\n",path);
	/**///方式内存泄露
	free(dir_name);
	free(file_name);
	return 0;
}

int fs_rename(const char *oldpath, const char *newname)
{
	//解析路径
	printf("we start to rename\n");
	printf("	old path is %s\n new path is  %s\n", oldpath, newname);
	//fflush(stdout);
	int file_inode_number = understand_the_path(oldpath);
	//先读取目录
	char * old_dir = (char*)calloc(sizeof(oldpath), sizeof(char));
	char * new_dir = (char*)calloc(sizeof(newname), sizeof(char));
	int i = 0;
	int old_dir_length = sizeof(oldpath)-1;
	int new_dir_length = sizeof(newname)-1;
	while (oldpath[old_dir_length] != '/')
		old_dir_length--;
	while (newname[new_dir_length] != '/')
		new_dir_length--;
	//进行了目录解析
	for (int i = 0; i < old_dir_length; i++)
		old_dir[i] = oldpath[i];
	for (int i = 0; i < new_dir_length; i++)
		new_dir[i] = newname[i];
	//目录读取完毕
	printf("	old dir is %s\n new dir is %s\n",old_dir,new_dir);
	//获取文件的位置
	int old_dir_inode_number = understand_the_path(old_dir);
	iNode old_dir_inode = just_read_inode_from_disk(old_dir_inode_number);
	int file_amount = old_dir_inode.st_size / 32;
	DictioryContent we_want;
	for (int i = 0; i < file_amount; i++)
	{
		//获得文件的备份
		we_want = get_filename_data_from_dir_in_rank_view(i, old_dir_inode);
		if (we_want.inode_number == file_inode_number)//如果相等的话,删除原来的
		{
			delete_file_according_to_rank(i, old_dir_inode);
			break;
		}
	}
	//！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！
	//不妨不进行写入试一下
	//进行写入操作
	/**///现在已知的是we_want是一个文件的指示信息，文件的，只需要将其挂载在新的目录上
	int new_dir_inode_number = understand_the_path(new_dir);
	iNode new_inode = add_file_to_dict( just_read_inode_from_disk(new_dir_inode_number), we_want);
	repalce_iNode_according_to_disk_number(new_dir_inode_number, new_inode);
	//更新状态*/
	old_dir_inode.st_size -= 32;
	old_dir_inode.ctime = time(NULL);
	old_dir_inode.mtime = time(NULL);
	repalce_iNode_according_to_disk_number(old_dir_inode_number,old_dir_inode);
	//暂时完成了
	printf("Rename is called:%s\n", oldpath);
	//printf("i don;t konw waht happend !!!!!!!!!!\n");
	free(old_dir);
	free(new_dir);
	return 0;
}


/**///进行粗粒度的整块写，传进去的都是大小为4096的块，如果有有偏移，那么最开头的元素一定会被初始化成为0

/**///已经完成
/**/void write_into_disk_acccording_to_rank_in_inode(int rank,iNode new_file_node,char *content)
{
	if (rank < 12)
		disk_write(datablock_find_disk_id(new_file_node.direct_pointer[rank]), content);
	else
	{
		//判断实在第几个间接
		int index = rank / (1024 + 12);
		int transdfer_rank = rank - index * (1024)-12;
		int data_block_number = new_file_node.indirect_pointer[index];
		//将间接指向的1024个data block 编号读出来
		char * real_data_node_number[1024];
		disk_read(datablock_find_disk_id(data_block_number), real_data_node_number);
		data_block_number = real_data_node_number[transdfer_rank];
		//写进去
		disk_write(datablock_find_disk_id(data_block_number), content);

	}

}
/**///目的是将新申请的datablock挂载到原来的文件中
/**/iNode mount_block_to_inode_according_to_rank(int rank, int data_black_number, iNode now_file_node)
{
	if (rank < 12)
		now_file_node.direct_pointer[rank] = data_black_number;
	else
	{
		int index = rank / (1024 + 12);
		int bias = rank - index * 1024 - 12;
		int buffer_number[1024];
		//读出来间接指向的块
		disk_read(datablock_find_disk_id(now_file_node.indirect_pointer[index]), buffer_number);
		//进行修改
		buffer_number[bias] = data_black_number;
		//写回去
		disk_write(datablock_find_disk_id(now_file_node.indirect_pointer[index]), buffer_number);
	}
	return now_file_node;
}
/**///目前已经写完了，但是read_write没有调试
/**///这里的buffer_node中的最后一个是‘\n’符号，这是系统自己多加的
/**///!!!!!!!!!!!!!目前的问题是虽然写到了块里，但是没有进行块和inode的连接，所以无法显示，这一点需要修改
/**///现在已经修改好了
void check_write(char * path)
{
	int iNode_number = understand_the_path(path);
	iNode here = just_read_inode_from_disk(iNode_number);
	int data_block_number = here.direct_pointer[0];
	char buffer_content[4096];
	disk_read(datablock_find_disk_id(data_block_number), buffer_content);
	printf("in checking process '");
	for (int i = 0; i < 5; i++)
		printf("%c ", buffer_content[i]);
	printf("'\n");
}
int fs_write (const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi)
{

	printf("	start to write \n");
	int disk_char_amount = 4096;
	//读取inode
	int iNode_number = understand_the_path(path);
	iNode new_file_inode = just_read_inode_from_disk(iNode_number);
	printf(" in writing : the inode number is %d\n", iNode_number);
	//确定开始的位置
	int pre_number_size = offset / disk_char_amount;
	int start_position = offset % disk_char_amount;
	int have_wrote_size = 0;
	int disk_rank = pre_number_size;
	if (start_position == 0)//先挂载
	{
		//申请新空间
		int tem_number = find_empty_datablock();
		make_data_map_covered(tem_number);
		//挂载上
		new_file_inode = mount_block_to_inode_according_to_rank(pre_number_size,tem_number, new_file_inode);
	}

	/*先进行第一个的写*/
	/**///总之先把第一个读进来
	char first_wirte_in[4096];
	read_the_one_disk_according_to_rank_in_inode(pre_number_size, new_file_inode,first_wirte_in);
	/**///对于第一个磁盘进行写操作
	printf("in writing  :the content is '");
	for (int i = 0; i < (int)size; i++)
		printf("%c ", buffer[i]);
	printf("'\n");
	printf("in writing : start position is %d\n", start_position);
	printf("in writing : first write size is %d\n", sizeof(first_wirte_in));
	printf("in writing : buffer size is %d \n", sizeof(buffer));
	for (int i = (int)start_position; i < disk_char_amount; i++)
	{
		printf("%c ",buffer[have_wrote_size]);
		//总的大小不能超过
		if (have_wrote_size >= ((int)(size) -1))
			break;
		//否则在后面接着写
		else
		{
			//printf("in writing ,first_write_in index =%d , buffer_index is %d \n", i, have_wrote_size);
			first_wirte_in[i] = buffer[have_wrote_size];
			have_wrote_size++;
		}
	}
	printf("\n");
	
	write_into_disk_acccording_to_rank_in_inode(pre_number_size, new_file_inode, first_wirte_in);
	printf("finish_first_rank");
	disk_rank++;
	/**///循环写入
	while (have_wrote_size<(int)size)
	{
		char  worte[4096] = { 0 };
		/**///按照规格写进去
		for (int i = 0; i < disk_char_amount; i++)
		{
			if (have_wrote_size <= size)
				worte[i] = buffer[have_wrote_size];
			else
				break;
			have_wrote_size++;
		}
		/**///将磁盘写进去
		/**///写之前先进行挂载
		int tem_number = find_empty_datablock();
		make_data_map_covered(tem_number);
		new_file_inode =   mount_block_to_inode_according_to_rank(disk_rank, tem_number, new_file_inode);
		//挂载之后写入
		write_into_disk_acccording_to_rank_in_inode(disk_rank, new_file_inode, worte);
		disk_rank++;

	}
	new_file_inode.st_size = (int)offset + (int)size;
	/**///修改这个iNode
	repalce_iNode_according_to_disk_number(iNode_number, new_file_inode);
	/**///修改这个文件的大小
	printf("start checking write !!!!!!!!!!!!!!\n");
	check_write(path);
	printf("end of checking!!!!!!!!!!!!!!!!!\n");

	printf("Write is called:%s\n",path);
	return (int)size;
}

/**/
/**///使用rank思路基本完成，等待测试
iNode delete_n_rank_block(int rank,iNode now_file_inode)
{
	int free_data_block_number;
	if (rank < 12)
		free_data_block_number = now_file_inode.direct_pointer[rank];
	else
	{
		int index = rank / (12 + 1024);
		//获得bias
		int transfer = rank - 12 - 1024 * index;
		int tem_data_block_number[1024];
		//读取数字
		disk_read(datablock_find_disk_id(now_file_inode.indirect_pointer[index]), tem_data_block_number);
		//真正datablock的number
		free_data_block_number = tem_data_block_number[transfer];

	}
	//进行删除
	make_data_map_uncovered(free_data_block_number);
	return now_file_inode;
}
iNode add_new_empty_block_according_to_rank(int rank , iNode now_file_number)
{
	if (rank < 12)
	{
		//申请并且标记
		int data_block_number = find_empty_datablock();
		make_data_map_covered(data_block_number);
		//进行inode的对应
		now_file_number.direct_pointer[rank] = data_block_number;
	}
	else
	{
		//进行预先的分配
		if (rank == 12)
		{
			//对于间接的指针分配空间
			int data_block_number = find_empty_datablock();
			make_data_map_covered(data_block_number);
			now_file_number.indirect_pointer[0] = data_block_number;
		}
		if (rank == (12 + 1024))
		{
			//进行和上述同样的操作
			int data_block_number = find_empty_datablock();
			make_data_map_covered(data_block_number);
			now_file_number.indirect_pointer[1] = data_block_number;
		}
		//进行写操作
		int index = rank / (1024 + 12);
		int bias = rank - index * 1024 - 12;
		int buffer_index[1024];//先读取出来
		disk_read(datablock_find_disk_id(now_file_number.indirect_pointer[index]), buffer_index);
		//对于读出来的东西进行扩充
		/**///申请空间
		int data_block_number = find_empty_datablock();
		make_data_map_covered(data_block_number);
		/**///进行标记
		buffer_index[bias] = data_block_number;
		//写回去
		disk_write(datablock_find_disk_id(now_file_number.indirect_pointer[index]), buffer_index);
	}
	//写回去
	return now_file_number;
}
int fs_truncate (const char *path, off_t size)
{
	//解析路径
	int inode_number = understand_the_path(path);
	//获取文件的细节信息
	int disk_id; int index;
	inode_find_disk_id_and_index_in_disk(inode_number, &index, &disk_id);
	iNode buffer_inode[32];
	disk_read(disk_id, buffer_inode);
	off_t pre_size = buffer_inode[index].st_size;
	int pre_block_size = (int)pre_size / 4096 +1;
	int now_block_size = (int)size / 4096 + 1;
	//不需要对datablock进行处理
	if (pre_block_size == now_block_size)
		buffer_inode[index].st_size = size;
	else//需要进行处理
	{
		if (now_block_size > pre_block_size)//需要进行申请操作
		{
			for (int i = now_block_size + 1; i <= pre_block_size; i++)
				buffer_inode[index] = add_new_empty_block_according_to_rank(i, buffer_inode[index]);
			/*
				pre block 有三种情况 0.1.2
				now block 有三种情况，0，1，2
				共有【pre,next】 0,0; 0,1;0,2;1,1;1,2;2,2,六种情况
			*/
		}
		else//需要进行删除操作
		{
			for (int i = (now_block_size + 1); i <= pre_block_size; i++)
				buffer_inode[index] = delete_n_rank_block(i, buffer_inode[index]);
			/*
				pre block 有三种情况 0.1.2
				now block 有三种情况，0，1，2
				共有【pre,next】 0，0 ； 1，0；1，1； 2，1；2，0；2，2，六种情况
			*/
		}
	}
	//对于size进行改变
	buffer_inode[index].st_size = size;
	//写回去
	disk_write(disk_id, buffer_inode);
	printf("Truncate is called:%s\n",path);
	return 0;
}
/**///在touch的时候调用，完成度较高
int fs_utime (const char *path, struct utimbuf *buffer)
{
	int inode_number = understand_the_path(path);
	int disk_id; int index;
	inode_find_disk_id_and_index_in_disk(inode_number, &index, &disk_id);
	iNode buffer_inode[32];
	disk_read(disk_id, buffer_inode);
	buffer_inode[index].ctime = time(NULL);
	buffer->actime = buffer_inode[index].atime;
	buffer->modtime = buffer_inode[index].ctime;
	disk_write(disk_id, buffer_inode);
	printf("Utime is called:%s\n",path);
	return 0;
}

int get_free_block_size()
{
	int free_number = 0;
	char bufffer_first[4096];
	char buffer_second[4096];
	//将data block 的标志位全部读出来
	disk_read(2, bufffer_first);
	disk_read(3, buffer_second);
	for (int i = 0; i < 4096; i++)
	{
		for (int k = 0; k < 8; k++)
		{
			if (bufffer_first[i] & bit_map_mask[k] == 0)
				free_number++;
			if (buffer_second[i] & bit_map_mask[k] == 0)
				free_number++;
		}
	}
	return free_number;
}
int get_file_number()
{
	int file_amount = 0;
	char number_buffer[4096];
	disk_read(1, number_buffer);
	for (int i = 0; i < 4096; i++)
	{
		for (int k = 0; k < 8; k++)
		{
			if (number_buffer[i] & bit_map_mask[k] != 0)
				file_amount++;
		}
	}
	return file_amount;
}
int fs_statfs (const char *path, struct statvfs *stat)
{
	printf("we start to get the main info in the disk\n");
	//一些不变的量
	stat->f_bsize = 4096;//块的大小
	stat->f_blocks = 256 * 256;//块的数量
	stat->f_namemax = 24;
	//需要进行扫描的量
	/**///先就数据块的角度而言
	stat->f_bfree = get_free_block_size();
	stat->f_bavail = get_free_block_size();
	/**///再就文件而言
	stat->f_files = get_file_number();
	stat->f_ffree = 4096 - get_file_number();
	stat->f_favail = 4096 - get_file_number();
	printf("Statfs is called:%s\n",path);
	return 0;
}


/**///打开文件，请教了陈大佬之后，他告诉的不需要做任何事情
int fs_open (const char *path, struct fuse_file_info *fi)
{
	int iNode_number = understand_the_path(path);
	iNode file_inode  = just_read_inode_from_disk(iNode_number);
	/*
	本来的目的是判断O_APPEND，并在fi->fh 中记录对应的信息
	但是这里系统自己做了对应的处理
	所以暂时不需要要做任何事
	*/
	printf("Open is called:%s\n",path);
	return 0;
}

//Functions you don't actually need to modify
int fs_release (const char *path, struct fuse_file_info *fi)
{
	printf("Release is called:%s\n",path);
	return 0;
}

int fs_opendir (const char *path, struct fuse_file_info *fi)
{
	printf("Opendir is called:%s\n",path);
	return 0;
}

int fs_releasedir (const char * path, struct fuse_file_info *fi)
{
	printf("Releasedir is called:%s\n",path);
	return 0;
}

static struct fuse_operations fs_operations = {
	.getattr    = fs_getattr,
	.readdir    = fs_readdir,
	.read       = fs_read,
	.mkdir      = fs_mkdir,
	.rmdir      = fs_rmdir,
	.unlink     = fs_unlink,
	.rename     = fs_rename,
	.truncate   = fs_truncate,
	.utime      = fs_utime,
	.mknod      = fs_mknod,
	.write      = fs_write,
	.statfs     = fs_statfs,
	.open       = fs_open,
	.release    = fs_release,
	.opendir    = fs_opendir,
	.releasedir = fs_releasedir
};

int main(int argc, char *argv[])
{
	if(disk_init())
		{
		printf("Can't open virtual disk!\n");
		return -1;
		}
	if(mkfs())
		{
		printf("Mkfs failed!\n");
		return -2;
		}
	return fuse_main(argc, argv, &fs_operations, NULL);
}
