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
#define MAX_FILENAME 8           /* �ļ�������󳤶�*/
#define MAX_EXTENSION 3         /* �ļ���չ������󳤶� */
#define DATABLOCK_SIZE 1024 /*4kb*/
#define MAX_FILE_NUMBER 32768
#define DerictPointer 12
#define InDerictPointer 2
#define Main_Direct_Inode_Position 4

/*��С����*/
typedef struct 
{
	mode_t  st_mode; //�ļ���Ӧ��ģʽ	4
	nlink_t st_nlink;//�ļ���������		8
	uid_t   st_uid;  //�ļ�������		4
	gid_t   st_gid;  //�ļ������ߵ���	4
	off_t   st_size; //�ļ��ֽ���		8
	time_t  atime;//�����ʵ�ʱ��		8
	time_t  mtime;//���޸ĵ�ʱ��		8
	time_t  ctime;//״̬�ı�ʱ��		8
	int direct_pointer[DerictPointer];
	int indirect_pointer[InDerictPointer];
	int id;// inode number �ı��
	char make_full[12];
	
}iNode;
typedef struct   {
	char file_name[28];
	//char file_extent[4];
	int inode_number;
}DictioryContent;
/*����inode bitmap��Ѱ��*/

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
/**///����Ѱ��һ���յ�inode,���ҿ��Կ������inode���ڵڼ���,
	//�����˳���Ǵ��㿪ʼ���Ժ���Ҳ�Ǵ��㿪ʼ
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
//���ص��Ǵ��̿�ź�buffer��inode��ʼ�ĵط�
void inode_find_disk_id_and_index_in_disk(int inode_number,int * index,int * disk_id)
{
	int Inode_Data_Start_disk_id = 4;
	int disk_index = inode_number / 32;
	(*index) = (inode_number % 32);
	(*disk_id) = Inode_Data_Start_disk_id + disk_index;
	return;
}
//����Ѿ���ռ��
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

/**///����data block
int find_empty_datablock()//ͨ��ͨ��map�ҵ�datalock��id
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
	//�����ݶ�ȡ����
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
/*��һ��д����ʼ��*/
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
	//����Ŀ¼�ı�ű�ǳ�Ϊ�Ѿ�ʹ��
	make_inode_map_covered(Main_Direct_Inode_Position);
	//��λ��Ŀ¼����Ӧ��inode
	int index; int disk_id;
	//���ձ���ҵ����Ӧ�Ĵ��̺�ƫ����
	inode_find_disk_id_and_index_in_disk(Main_Direct_Inode_Position, &index, &disk_id);
	iNode buffer_inode[32];
	disk_read(disk_id, buffer_inode);
	//����Ŀ¼�е�inode��ֵ
	give_the_value_to_inode(buffer_inode, index);
	disk_write(disk_id, buffer_inode);
	//����Ŀ¼д��ȥ����ʱ����û�õ�data_block
	//��ʼ����--���Գɹ���δ����Խ������--����get -arr

	return 0;
}

/**/iNode just_read_inode_from_disk(int inode_number)
{
	int index; int disk_id;
	//���ձ���ҵ����Ӧ�Ĵ��̺�ƫ����
	inode_find_disk_id_and_index_in_disk(inode_number, &index, &disk_id);
	iNode buffer_inode[32];
	disk_read(disk_id, buffer_inode);
	return buffer_inode[index];
}
/**/void repalce_iNode_according_to_disk_number(int inode_number,iNode new_file_node)
{
	int index; int disk_id;
	//���ձ���ҵ����Ӧ�Ĵ��̺�ƫ����
	inode_find_disk_id_and_index_in_disk(inode_number, &index, &disk_id);
	iNode buffer_inode[32];
	disk_read(disk_id, buffer_inode);
	/**///���и�д
	buffer_inode[index] = new_file_node;
	//д��ȥ����
	disk_write(disk_id, buffer_inode);
	return;


}
/**///��Ŀ¼֮��Ѱ���ļ��������ļ���inode
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
	//���ļ������н���
	int extent_size = 0;
	for (int i = son_name_size - 1; i >= 0; i--)
	{
		if (son_name[i] != '.')
			extent_size++;
		else
			break;
	}
	//������Ǵ�С��ͬ��֤��û����չ��
	if (extent_size == son_name_size)
		extent_size = -1;//������-1����Ϊ��ʹ�ú�������name_size����Ҫ��ȥ���Ǹ�.�ܵ���Ӱ��õ�����
	int name_size = son_name_size - extent_size - 1;
	int extent_start = name_size + 1;
	//���ƵıȽ�
	int flag = 0;
	for (int i = 0; i < name_size; i++)
	{
		if (name_we_get.file_name[i] != son_name[i])
			return flag;
	}
	//��ֹ����ƥ����жദ
	if (name_we_get.file_name[name_size] != 0)
		return flag;
	//���������ͬ����չ��û�У���ôҲ��ok�ġ�
	if (extent_size == -1)
		return 1;
	//��չ���ıȽ�
	for (int i = extent_start; i < son_name_size; i++)
	{
		if (name_we_get.file_extent[i - extent_start] != son_name[i])
			return flag;
	}
	//��ֹ�������ĳ���
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
			if (visited_file >= file_size)//�����������û���ڶ�Ӧ��Ŀ¼���ҵ��ļ�
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
			if (file_size <= visited_file)//�߽�����--������������Χ
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
			if (file_size <= visited_file)//�߽�����--������������Χ
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
	//�������ƽ��н���
	//������ô����ļ�
	int file_size = (int)father_inode.st_size/32;
	//int disk_size = file_size / 128 + 1;//��Ҫ��ô������洢
	//int visited_file_number = 0;
	//�Ƚ���ֱ�ӷ���
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
	//�ж��Ƿ�Ҫ���е�һ�μ�ӷ���
	if ((file_size > DerictPointer * 128)&&(result== -ENOENT))
	{
		result = visit_first_indirect_to_get_file(file_size, father_inode, son_name, son_name_size);
		//�ж��Ƕ�Ҫ���еڶ��μ�����
		if ((file_size > DerictPointer * 128 + 1024 * 128)&&(result== -ENOENT))
			result = visit_second_indirect_to_get_file(file_size, father_inode, son_name, son_name_size);
		/*disk_size -= 12;
		//����ָ�����ݿ�ı��
		int buffer_data_number[1024];
		for (int i = 0; i < 2; i++)
		{
			//ָ����1024�����ݿ��
			disk_read(datablock_find_disk_id(father_inode.indirect_pointer[i]),buffer_data_number);
			int index_count = 0;
			while ((index_count < 1024) && (visited_file_number < file_size))
			{
				//��ȡһ�����ݿ��Ӧ�������ļ�����
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
	//�������յĽ��
	return result;
}

//����ֵ�������ļ����ڵ�inode�ı��
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
	//�õ���inode
	iNode father_inode = just_read_inode_from_disk(Main_Direct_Inode_Position);
	/*�õ��˸�Ŀ¼���ļ�����*/
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
		//�õ�һ������Ϊson_name_size������
		//�ж����ļ�����Ŀ¼
		if (iteration == length)//�����յ��ļ���
		{
			result_number = find_file_in_dict(father_inode, son_name, son_name_size);
		}
		else//��Ŀ¼
		{
			int time_inode_number = find_file_in_dict(father_inode, son_name, son_name_size);
			//��������һ���򵥵ĸĶ���
			father_inode = just_read_inode_from_disk(time_inode_number);
			iteration++;//������Ϊ��������������/���������
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
/*cd *//*�ڶ���д*/
int fs_getattr (const char *path, struct stat *attr)
{
	//��ԭ��stat�е���������
	printf("start to getatr\n");
	memset(attr, 0, sizeof(struct stat));
	//�ҵ��ļ����ڵ�iNode
	int iNode_Number = understand_the_path(path);
	if (iNode_Number == -ENOENT)
		return iNode_Number;
	//��·���ж�����
	iNode aim_inode = just_read_inode_from_disk(iNode_Number);
	printf("	the inode number we get  in get_arr is %d\n", iNode_Number);
	//���и�ֵ����
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
//�Ѿ����ǹ����ˣ�����ֻ֧��ֱ��ָ����ɶȲ��Ǻܸ�
//�Ѿ����¸�д���㹻֧��ȫ�ֵķ����ˣ�ֱ�Ӻͼ�Ӷ��У�
int fs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	//����·��
	int iNode_number = understand_the_path(path);
	printf("the read_dir number is %d\n",iNode_number );
	//��ȡ���Ŀ¼��Ӧ��inode
	iNode aim_node = just_read_inode_from_disk(iNode_number);
	int visited_size = 0;
	int file_size = aim_node.st_size / 32;
	printf("the read_dir file_number is %d\n", file_size);
	DictioryContent tem_buffer[128];
	disk_read(datablock_find_disk_id(aim_node.direct_pointer[0]), tem_buffer);
	//printf("in reading the name is %s?????????????????????", tem_buffer[0].file_name);
	int disk_size = file_size / 128+1;
	//��inode�ж�ȡ����
	///**/ֱ�Ӷ�ȡ
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
			//��һ���Ž�ȥһ��
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

/**///����inode��datablock�ŵڼ����ӵ�һ������DerictPointer + 1024 *2��������������Ӧ��4K�����ݿ������
/**/// �������������Ҫ����free��--�����˴��淽ʽ���Ѿ�����Ҫ��
/**///����ֵ��һ����Ϊ4096������
/**/void  read_the_one_disk_according_to_rank_in_inode(int rank,iNode now_file_inode,char * content_buffer)
{
	//char  content_buffer[4096];
	if (rank < 12)
		disk_read(datablock_find_disk_id(now_file_inode.direct_pointer[rank]), content_buffer);
	else
	{
		//�ж�ʵ�ڵڼ������
		int index = rank / (1024 + 12);
		int transdfer_rank = rank - index * (1024 ) - 12;
		int data_block_number = now_file_inode.indirect_pointer[index];
		//�����ָ���1024��data block ��Ŷ�����
		char * real_data_node_number[1024];
		disk_read(datablock_find_disk_id(data_block_number), real_data_node_number);
		data_block_number = real_data_node_number[transdfer_rank];
		//���������ݶ�����
		disk_read(datablock_find_disk_id(data_block_number), content_buffer);

	}
	return ;

}
//Ŀǰ�Ѿ�д����ˣ����ǻ�û�о�������
//�ȴ������fs_write֮����й�ͬ�Ĳ���
int fs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi)
{
	printf("important !!!!!!!!!!!!!!!!!!!!!!:  start to read\n");
	//buffer = (char*)calloc((int)size, sizeof(char));
	int buffer_char_size = 4096;//����֮�����
	/*
	�Ƚ��е�һ������offset��������ܲ��ܶ���Ĵ��̵Ķ�ȡ
	�ڽ����м�Ķ�ȡ
		��ȡҪ����ֱ�ӵ�
		���ǵ�һ�μ��
		���ǵڶ��μ��
	���������һ�����̵Ķ�ȡ�����һ�����̵Ķ�ȡ�����ܻ᲻��������
	*/
	//��ȡ����ļ���Ӧ��inode
	int iNode_number = understand_the_path(path);
	printf("reading file : inode number is %d\n", iNode_number);
	iNode now_file_inode = just_read_inode_from_disk(iNode_number);
	//�ų��������
	
	/*if ((int)size + (int)offset > (int)(now_file_inode.st_size))
		return EOF;*/
	//��һ�¶Ա�һ���������̺���
	int sum_content_disk_amount = (int)now_file_inode.st_size / buffer_char_size+ 1;
	int start_disk_rank = offset / buffer_char_size;//�ӵڼ�����̿�ʼ�Ķ�
	int start_bias = offset % buffer_char_size;//��һ���Ķ���ƫ�����Ƕ���
	int have_read_size = 0;//�Ѿ��Ķ���������
	//��ʼѭ���Ķ�
	printf("in reading the size we ask is %d", size);
	while (have_read_size != (int)size)
	{
		//�򿪿�ʼ��Ŷ�Ӧ�Ĵ��̵�����
		char tem_read_buffer[4096];
		read_the_one_disk_according_to_rank_in_inode(start_disk_rank, now_file_inode,tem_read_buffer);
		//�����Ķ�
		for (int i = start_bias; i < buffer_char_size; i++)
		{
			//printf("in reading : we read %c\n", tem_read_buffer[i]);
			buffer[have_read_size] = tem_read_buffer[i];
			have_read_size++;
			if (have_read_size == (int)size)
				break;
		}
		//�¿�һ����̣�ͨ��˳�ӵķ�ʽ��
		start_disk_rank++;
		//�¿��ٵ�һ��Ҫ��ͷ��ʼ��ȡ
		start_bias = 0;
	}

	for (int i = 0; i < 5; i++)
		printf("in reading :here is the result %c\n", buffer[i]);
	//��ȡ���̽���
	printf("Read is called:%s\n",path);
	//free(tem_member_buff);
	printf("in reading : len is %d\n", get_min((int)size, (int)now_file_inode.st_size));
	return get_min((int)size, (int)now_file_inode.st_size);
}

/**/ int apply_a_new_inode_and_initial()
{
	//�ҵ�û�б�ռ�õĿռ�
	int new_inode_number = find_empty_inode();
	//���Ϊ�Ѿ�ռ��
	make_inode_map_covered(new_inode_number);
	//�ҵ��������inode��Ӧ�Ĵ��̲��Ҷ�ȡ����
	int index, disk_id;
	inode_find_disk_id_and_index_in_disk(new_inode_number, &index, &disk_id);
	iNode inode_buffer[32];
	disk_read(disk_id, inode_buffer);
	//��Ӧ��inodeΪinode_buffer[index]
	//����������д��
	inode_buffer[index].st_mode = REGMODE;
	inode_buffer[index].st_nlink = 1;
	inode_buffer[index].st_uid = getuid();
	inode_buffer[index].st_gid = getgid();
	inode_buffer[index].st_size = 0;
	inode_buffer[index].atime = time(NULL);
	inode_buffer[index].mtime = time(NULL);
	inode_buffer[index].ctime = time(NULL);
	inode_buffer[index].id = new_inode_number;
	//д��֮��ˢ�¹���Ķ���д��ȥ
	disk_write(disk_id, inode_buffer);
	return new_inode_number;
	
}
/**/ int apply_a_new_inode_and_initial_as_dict()
{
	//�ҵ�û�б�ռ�õĿռ�
	int new_inode_number = find_empty_inode();
	//���Ϊ�Ѿ�ռ��
	make_inode_map_covered(new_inode_number);
	//�ҵ��������inode��Ӧ�Ĵ��̲��Ҷ�ȡ����
	int index, disk_id;
	inode_find_disk_id_and_index_in_disk(new_inode_number, &index, &disk_id);
	iNode inode_buffer[32];
	disk_read(disk_id, inode_buffer);
	//��Ӧ��inodeΪinode_buffer[index]
	//����������д��
	inode_buffer[index].st_mode = DIRMODE;
	inode_buffer[index].st_nlink = 1;
	inode_buffer[index].st_uid = getuid();
	inode_buffer[index].st_gid = getgid();
	inode_buffer[index].st_size = 0;
	inode_buffer[index].atime = time(NULL);
	inode_buffer[index].mtime = time(NULL);
	inode_buffer[index].ctime = time(NULL);
	inode_buffer[index].id = new_inode_number;
	//д��֮��ˢ�¹���Ķ���д��ȥ
	disk_write(disk_id, inode_buffer);
	return new_inode_number;
}
//��������ļ������datablock�е�id��
/**//**/void add_a_dict_in_data_block(int data_block_node_number,DictioryContent aim_file,int file_position)
{
	
	//��datablock��Ӧ��Ӳ�̶�����
	int disk_id = datablock_find_disk_id(data_block_node_number);
	DictioryContent file_buffer[128];
	disk_read(disk_id, file_buffer);
	//����Ӧ�ı����д����
	file_buffer[file_position] = aim_file;
	//д��ȥ
	disk_write(disk_id, file_buffer);
	//����״̬���

	return;
}
//Ŀ���ǽ��ļ�������Ŀ¼�ϣ�����ֻ�޸���һ�㣬���һ�û�б��״̬
//���ҽ���״̬���
/**/ void  read_the_one_disk_according_to_rank(int rank, iNode now_file_inode, DictioryContent * content_buffer)
{
	//char  content_buffer[4096];
	if (rank < 12)
		disk_read(datablock_find_disk_id(now_file_inode.direct_pointer[rank]), content_buffer);
	else
	{
		//�ж�ʵ�ڵڼ������
		int index = rank / (1024 + 12);
		int transdfer_rank = rank - index * (1024) - 12;
		int data_block_number = now_file_inode.indirect_pointer[index];
		//�����ָ���1024��data block ��Ŷ�����
		char * real_data_node_number[1024];
		disk_read(datablock_find_disk_id(data_block_number), real_data_node_number);
		data_block_number = real_data_node_number[transdfer_rank];
		//���������ݶ�����
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
		//�ж�ʵ�ڵڼ������
		int index = rank / (1024 + 12);
		int transdfer_rank = rank - index * (1024) - 12;
		int data_block_number = new_file_node.indirect_pointer[index];
		//�����ָ���1024��data block ��Ŷ�����
		char * real_data_node_number[1024];
		disk_read(datablock_find_disk_id(data_block_number), real_data_node_number);
		data_block_number = real_data_node_number[transdfer_rank];
		//д��ȥ
		disk_write(datablock_find_disk_id(data_block_number), content);

	}

}
/**/iNode add_file_to_dict(iNode dict_inode,DictioryContent aim_file)
{

	/*
	ÿ��inode�ж�ָ��ͬ��datablock
	��Ҫ�жϼ��������ֱ��ָ���Ǽ�ӣ�ʹ��disk_size�����жϣ�
	������ֱ��ʹ���У������ڵ�һ�μ�飬�����ǵڶ��μ��
	��ÿһ��������������ֿ����ԣ�һ������Ҫ�����µ�datablock
	��һ���ǲ���Ҫ�����µ�datablock,�������ͨ��file_number�������ж�
	*/
	//���ڵ��ļ�����
	/*int now_file_size = dict_inode.st_size / 32;
	int disk_amount = now_file_size / 128;
	int disk_bias = now_file_size % 128;
	//�ȴ���Ҫ��������,�����ڼ�ӵķֲ��
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
	//�ڴ�����֮��,�����µ����ݿ�,�����¹�
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
	//����֮�󣬰�˳��ȥ��ȡ
	DictioryContent content_buffer[128];
	read_the_one_disk_according_to_rank(disk_amount, dict_inode, content_buffer);
	content_buffer[disk_bias] = aim_file;
	write_into_disk_acccording_to_rank(disk_amount, dict_inode, content_buffer);

	*/
	int pre_file_number = dict_inode.st_size / 32;
	//����ʹ�õĴ�������
	int pre_disk_number = pre_file_number / 128;

	printf("		pre_file_number is %d\n", pre_file_number);
	if (pre_disk_number < 12)//֤��û��ȫ��ռ��
	{
		if (pre_file_number % 128 == 0)//��Ҫ�����µĿռ�
		{
			//����һ���µ�datablock���ұ��
			int data_block_node_number = find_empty_datablock();
			make_data_map_covered(data_block_node_number);
			printf("			the new_get_data_block_number is %d\n", data_block_node_number);
			//���������datablock����Ϊ����Ŀ¼�µ��ļ���Ϣ�����ڵ�
			dict_inode.direct_pointer[pre_file_number / 128] = data_block_node_number;
			//�������������ǰ��ļ���Ϣд���½������ݿ�֮��
			add_a_dict_in_data_block(data_block_node_number, aim_file, 0);
		}
		else//ֱ���ں�����Ͼͺ�
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
	//���޸����֮����״̬
	dict_inode.st_size += 32;
	dict_inode.mtime = time(NULL);
	dict_inode.ctime = time(NULL);
	return dict_inode;
	//if((pre_disk_number>=12)&&(pre_disk_number))
}
//ȱ�ٺ�����û�в���


//��������ȫ����mkdir����,
/*
����汾�޸��˳�ʼ���ĺ���
��apply_a_new_inode_and_initial_as_dict()
��ʼ������apply_a_new_inode_and_initial_as_dict

*/
int fs_mknod(const char *path, mode_t mode, dev_t dev)
{
	//���·���Ĵ�С
	int length = 0;
	while (path[length] != 0)
		length++;
	//Ҫд���ļ����ļ����Ĵ�С
	int last_file_name_length = 0;
	while (path[length - last_file_name_length - 1] != '/')
	{
		last_file_name_length++;
	}
	char * dict_path = (char*)calloc(length + 10, sizeof(char));
	char file_name[30] = { 0 };
	//�õ��ļ���������Ϊһ���������ַ���
	int start = length - last_file_name_length;
	for (int i = start; i < length; i++)
		file_name[i - start] = path[i];
	//�õ��ļ�·��--ͨ������ļ����ķ�ʽ
	printf("	the dict name length is %d\n", length - last_file_name_length - 1);
	if (length - last_file_name_length - 1 == 0)
		dict_path[0] = '/';
	for (int i = 0; i < length - last_file_name_length - 1; i++)
		dict_path[i] = path[i];


	//�õ���Ŀ¼�ж�Ӧ��inode_number
	int father_inode_number = understand_the_path(dict_path);
	printf("	the dict_inode number is %d\n", father_inode_number);
	//Ϊһ���ļ���Ϣ���г�ʼ��
	DictioryContent new_file;
	for (int i = 0; i < 28; i++)
		new_file.file_name[i] = file_name[i];
	new_file.inode_number = apply_a_new_inode_and_initial();
	printf("	new inode _number is %d ��������������������������������������\n", new_file.inode_number);
	printf("	new dict name is' ");
	for (int i = 0; i < last_file_name_length; i++)
		printf("%c", new_file.file_name[i]);
	printf("'\n");


	printf("	the inode_number of dict is %d\n", father_inode_number);
	//����û�д���

	//���ļ����ص�Ŀ¼�ϡ�
	/**///���Ƚ�Ŀ¼��inode��ȡ����
	int disk_id, index;
	inode_find_disk_id_and_index_in_disk(father_inode_number, &index, &disk_id);
	iNode buffer_inode[32];
	disk_read(disk_id, buffer_inode);

	/**///�ڽ��ļ���Ϣ�ҵ�Ŀ¼����
	printf("	start to mount\n");
	buffer_inode[index] = add_file_to_dict(buffer_inode[index], new_file);
	printf("	after mounting ,we test store data %d\n", buffer_inode[index].direct_pointer[0]);
	disk_write(disk_id, buffer_inode);
	
	/*
	��ʼ���Զ�Ӧ�����ݿ��
	*/
	int test_id = datablock_find_disk_id(buffer_inode[index].direct_pointer[0]);
	printf("	test_id is %d\n", test_id);
	DictioryContent test_content[128];
	disk_read(test_id, test_content);
	printf("	test_after_create :%s\n", test_content[0].file_name);
	//printf("	test file inode is %d\n", test_content[0].inode_number);
	//printf("finish add\n");
	//�ڽ�Ŀ¼��inodeд��ȥ
	
	//��ֹ�ڴ�й©
	free(dict_path);
	//��ʱ��ֹ����
	if (buffer_inode[index].st_size / 32 > 128 * DerictPointer)
		return -ENOSPC;
	printf("Mknod is called:%s\n",path);
	return 0;
}
/**/
int fs_mkdir (const char *path, mode_t mode)
{
	//���·���Ĵ�С
	int length = 0;
	while (path[length] != 0)
		length++;
	//Ҫд���ļ����ļ����Ĵ�С
	int last_file_name_length = 0;
	while (path[length - last_file_name_length - 1] != '/')
	{
		last_file_name_length++;
	}
	char * dict_path = (char*)calloc(length + 10, sizeof(char));
	char file_name[30] = {0};
	//�õ��ļ���������Ϊһ���������ַ���
	int start = length - last_file_name_length;
	for (int i = start; i < length; i++)
		file_name[i - start] = path[i];
	//�õ��ļ�·��--ͨ������ļ����ķ�ʽ
	printf("	the dict name length is %d\n", length - last_file_name_length - 1);
	if (length - last_file_name_length - 1 == 0)
		dict_path[0] = '/';
	for (int i = 0; i < length - last_file_name_length - 1; i++)
		dict_path[i] = path[i];

	//�õ���Ŀ¼�ж�Ӧ��inode_number
	int father_inode_number = understand_the_path(dict_path);
	printf("	the dict_inode number is %d\n", father_inode_number);
	//Ϊһ���ļ���Ϣ���г�ʼ��
	DictioryContent new_file;
	for (int i = 0; i < 28; i++)
		new_file.file_name[i] = file_name[i];
	new_file.inode_number = apply_a_new_inode_and_initial_as_dict();
	printf("	new inode _number is %d ��������������������������������������\n", new_file.inode_number);
	printf("	new dict name is' ");
	for (int i = 0; i < last_file_name_length; i++)
		printf("%c",new_file.file_name[i]);
	printf("'\n");


	printf("	the inode_number of dict is %d\n", father_inode_number);
	//����û�д���
	//���ļ����ص�Ŀ¼�ϡ�
	/**///���Ƚ�Ŀ¼��inode��ȡ����
	int disk_id, index;
	inode_find_disk_id_and_index_in_disk(father_inode_number, &index, &disk_id);
	iNode buffer_inode[32];
	disk_read(disk_id, buffer_inode);

	/**///�ڽ��ļ���Ϣ�ҵ�Ŀ¼����
	printf("	start to mount\n");
	buffer_inode[index] = add_file_to_dict(buffer_inode[index], new_file);
	printf("	after mounting ,we test store data %d\n", buffer_inode[index].direct_pointer[0]);

	//д��ȥ
	disk_write(disk_id, buffer_inode);
	//����

	int test_id = datablock_find_disk_id(buffer_inode[index].direct_pointer[0]);
	printf("	test_id is %d\n", test_id);
	DictioryContent test_content[128];
	disk_read(test_id, test_content);
	printf("	test_after_create :%s\n", test_content[0].file_name);
	//��ֹ�ڴ�й©
	free(dict_path);
	//��ʱ��ֹ����
	if (buffer_inode[index].st_size / 32 > 128 * DerictPointer)
		return -ENOSPC;

	printf("Mkdir is called:%s\n",path);
	return 0;
}
/**//**/ void release_file_databock_by_provide_inode(iNode aim_inode)
{
	int datablock_amount = aim_inode.st_size / 4096 + 1;
	//���ͷ�ֱ�ӵĿ�
	for (int i = 0; i < get_min(12, datablock_amount); i++)
		make_data_map_uncovered(aim_inode.direct_pointer[i]);
	//���ͷż�ӵĿ�
	if (datablock_amount > 11)
	{
		int bias = datablock_amount - 11;
		int buffer_datablcok_member[1024];
		//���ж�ȡ����
		disk_read(datablock_find_disk_id(aim_inode.indirect_pointer[0]), buffer_datablcok_member);
		for (int i = 0; i < get_min(1024, bias); i++)
			make_data_map_uncovered(buffer_datablcok_member[i]);
	}
	//���еڶ�����ӵĿ���ͷŲ���
	if (datablock_amount > 11 + 1024)
	{
		int bias = datablock_amount - 11 - 1024;
		int buffer_datablcok_member[1024];
		disk_read(datablock_find_disk_id(aim_inode.indirect_pointer[1]), buffer_datablcok_member);
		for (int i = 0; i < get_min(1024, bias); i++)
			make_data_map_uncovered(buffer_datablcok_member[i]);
	}
	//���
	return;
}
/**/DictioryContent get_filename_data_from_dir_in_rank_view(int rank, iNode dir_inode)
{
	//ÿһ��Dict�Ĵ�С��32����һ����������128�������ӵĶ���
	int disk_rank = rank / 128;//���㿪ʼ
	int disk_bias = rank % 128;//Ҳ�Ǵ��㿪ʼ
	DictioryContent buffer_file_info[128];
	//��ȡ������
	if (rank < 12)
		disk_read(datablock_find_disk_id(dir_inode.direct_pointer[disk_rank]), buffer_file_info);
	else
	{
		int index = (rank - 11) / 1024;//����ǵڼ������
		int indirect_bias = rank - 11 - index * 1024;//�ٵڼ�������еĿ��ƫ����
		int buffer_datablock_buffer[1024];
		disk_read(datablock_find_disk_id(dir_inode.indirect_pointer[index]), buffer_datablock_buffer);
		disk_read(datablock_find_disk_id(buffer_datablock_buffer[indirect_bias]), buffer_datablock_buffer);
	}
	return buffer_file_info[disk_bias];
}

/**///��ʱ��ɣ�û�в��ԣ�������������������������������������������������
/**///�������
int fs_rmdir (const char *path)
{
	//����Ŀ¼���ڵ�·��
	int dir_iNode_number = understand_the_path(path);
	iNode dir_inode = just_read_inode_from_disk(dir_iNode_number);
	//����Ŀ¼���ڵ�Ŀ¼
	char * old_dir = (char *)calloc(sizeof(path), sizeof(char));
	int  old_dir_len = sizeof(path) - 1;
	while (path[old_dir_len] != '/')
		old_dir_len--;
	for (int i = 0; i < old_dir_len; i++)
		old_dir[i] = path[i];
	//����Ŀ¼�е��ļ������ͷ�
	int file_amount = dir_inode.st_size / 32;
	for (int i = 0; i < file_amount; i++)
	{
		//�õ�һ���ļ�����Ϣ
		DictioryContent file_info = get_filename_data_from_dir_in_rank_view(i, dir_inode);
		//ɾ���ļ�����Ӧ�����ݿ�
		release_file_databock_by_provide_inode(just_read_inode_from_disk(file_info.inode_number));
		//�ͷ�inode
		make_data_map_uncovered(file_info.inode_number);
	}
	//�ļ��ͷ����
	//�����ļ�ɾ��
	fs_unlink(path);
	printf("Rmdir is called:%s\n",path);
	return 0;
}


/**///ģ��������һ����˳��д��Ĳ�������detelte�е��ã�������������������������������������������������������
/**/void write_filename_intodir_in_rank_view(int rank, iNode dir_inode, DictioryContent aim_data)
{
	//ÿһ��Dict�Ĵ�С��32����һ����������128�������ӵĶ���
	int disk_rank = rank / 128;//���㿪ʼ
	int bias = rank % 128;//Ҳ�Ǵ��㿪ʼ
	DictioryContent buffer_file_info[128];
	//��ȡ������
	if (rank < 12)
	{
		disk_read(datablock_find_disk_id(dir_inode.direct_pointer[disk_rank]), buffer_file_info);
		printf("		formal name is %s\n",buffer_file_info[bias].file_name);
		buffer_file_info[bias] = aim_data;
		disk_write(datablock_find_disk_id(dir_inode.direct_pointer[disk_rank]), buffer_file_info);
	}
	else
	{
		int index = (rank - 11) / 1024;//����ǵڼ������
		int indirect_bias = rank - 11 - index * 1024;//�ٵڼ�������еĿ��ƫ����
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
	//�ж��Ƿ������һ��
	int file_amount = dir_inode.st_size / 32;
	if (rank == file_amount)
		return;
	//���ĩβ�ı��
	DictioryContent last_file = get_filename_data_from_dir_in_rank_view(file_amount - 1, dir_inode);
	printf("		when delete we get name %s\n",last_file.file_name);
	// ��ĩβд�뵽ԭ���ı��
	write_filename_intodir_in_rank_view(rank, dir_inode, last_file);
	return;
}
//�ͷ��������ݿ�����Ӧ��datablock

//��һ����ɣ��ȴ���
//��Ŀ¼�²����Ѿ����
int fs_unlink (const char *path)
{
	//����·������
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
	//�õ��� dir_name��file_name
	//���ļ�����Ѱַ
	//����·���������в���
	printf("	dir_name is %s\n", dir_name);
	printf("	file_name is %s\n ", file_name);

	int file_inode = understand_the_path(path);
	//����Ŀ¼����Ѱַ
	int dir_inode_number = understand_the_path(dir_name);
	iNode dir_iNode = just_read_inode_from_disk(dir_inode_number);
	int file_amount = dir_iNode.st_size / 32;
	//��Ĭ�Ͻ���ʽ��ȷ��
	printf("	the dir amount is %d\n", file_amount);
	for (int i = 0; i < file_amount; i++)
	{
		printf("	in the porcess we have many names like : %s \n", get_filename_data_from_dir_in_rank_view(i, dir_iNode).file_name);
		if (get_filename_data_from_dir_in_rank_view(i, dir_iNode).inode_number == file_inode)
		{
			printf("	the same file name we get is %s\n", get_filename_data_from_dir_in_rank_view(i, dir_iNode).file_name);
			//�ͷ����ݿ�
			release_file_databock_by_provide_inode(just_read_inode_from_disk(file_inode));
			//ɾ��iNode�ͽ�����λ
			/**///////���������ƶ�λ�õĵط�
			delete_file_according_to_rank(i, dir_iNode);
			break;
		}
	}

	make_inode_map_uncovered(file_inode);
	//���ڸ�Ŀ¼����ʱ����ºʹ�С
	dir_iNode.st_size -= 32;
	dir_iNode.ctime = time(NULL);
	dir_iNode.mtime = time(NULL);
	repalce_iNode_according_to_disk_number(dir_inode_number, dir_iNode);
	printf("Unlink is callded:%s\n",path);
	/**///��ʽ�ڴ�й¶
	free(dir_name);
	free(file_name);
	return 0;
}

int fs_rename(const char *oldpath, const char *newname)
{
	//����·��
	printf("we start to rename\n");
	printf("	old path is %s\n new path is  %s\n", oldpath, newname);
	//fflush(stdout);
	int file_inode_number = understand_the_path(oldpath);
	//�ȶ�ȡĿ¼
	char * old_dir = (char*)calloc(sizeof(oldpath), sizeof(char));
	char * new_dir = (char*)calloc(sizeof(newname), sizeof(char));
	int i = 0;
	int old_dir_length = sizeof(oldpath)-1;
	int new_dir_length = sizeof(newname)-1;
	while (oldpath[old_dir_length] != '/')
		old_dir_length--;
	while (newname[new_dir_length] != '/')
		new_dir_length--;
	//������Ŀ¼����
	for (int i = 0; i < old_dir_length; i++)
		old_dir[i] = oldpath[i];
	for (int i = 0; i < new_dir_length; i++)
		new_dir[i] = newname[i];
	//Ŀ¼��ȡ���
	printf("	old dir is %s\n new dir is %s\n",old_dir,new_dir);
	//��ȡ�ļ���λ��
	int old_dir_inode_number = understand_the_path(old_dir);
	iNode old_dir_inode = just_read_inode_from_disk(old_dir_inode_number);
	int file_amount = old_dir_inode.st_size / 32;
	DictioryContent we_want;
	for (int i = 0; i < file_amount; i++)
	{
		//����ļ��ı���
		we_want = get_filename_data_from_dir_in_rank_view(i, old_dir_inode);
		if (we_want.inode_number == file_inode_number)//�����ȵĻ�,ɾ��ԭ����
		{
			delete_file_according_to_rank(i, old_dir_inode);
			break;
		}
	}
	//������������������������������������������������������������������������������
	//����������д����һ��
	//����д�����
	/**///������֪����we_want��һ���ļ���ָʾ��Ϣ���ļ��ģ�ֻ��Ҫ����������µ�Ŀ¼��
	int new_dir_inode_number = understand_the_path(new_dir);
	iNode new_inode = add_file_to_dict( just_read_inode_from_disk(new_dir_inode_number), we_want);
	repalce_iNode_according_to_disk_number(new_dir_inode_number, new_inode);
	//����״̬*/
	old_dir_inode.st_size -= 32;
	old_dir_inode.ctime = time(NULL);
	old_dir_inode.mtime = time(NULL);
	repalce_iNode_according_to_disk_number(old_dir_inode_number,old_dir_inode);
	//��ʱ�����
	printf("Rename is called:%s\n", oldpath);
	//printf("i don;t konw waht happend !!!!!!!!!!\n");
	free(old_dir);
	free(new_dir);
	return 0;
}


/**///���д����ȵ�����д������ȥ�Ķ��Ǵ�СΪ4096�Ŀ飬�������ƫ�ƣ���ô�ͷ��Ԫ��һ���ᱻ��ʼ����Ϊ0

/**///�Ѿ����
/**/void write_into_disk_acccording_to_rank_in_inode(int rank,iNode new_file_node,char *content)
{
	if (rank < 12)
		disk_write(datablock_find_disk_id(new_file_node.direct_pointer[rank]), content);
	else
	{
		//�ж�ʵ�ڵڼ������
		int index = rank / (1024 + 12);
		int transdfer_rank = rank - index * (1024)-12;
		int data_block_number = new_file_node.indirect_pointer[index];
		//�����ָ���1024��data block ��Ŷ�����
		char * real_data_node_number[1024];
		disk_read(datablock_find_disk_id(data_block_number), real_data_node_number);
		data_block_number = real_data_node_number[transdfer_rank];
		//д��ȥ
		disk_write(datablock_find_disk_id(data_block_number), content);

	}

}
/**///Ŀ���ǽ��������datablock���ص�ԭ�����ļ���
/**/iNode mount_block_to_inode_according_to_rank(int rank, int data_black_number, iNode now_file_node)
{
	if (rank < 12)
		now_file_node.direct_pointer[rank] = data_black_number;
	else
	{
		int index = rank / (1024 + 12);
		int bias = rank - index * 1024 - 12;
		int buffer_number[1024];
		//���������ָ��Ŀ�
		disk_read(datablock_find_disk_id(now_file_node.indirect_pointer[index]), buffer_number);
		//�����޸�
		buffer_number[bias] = data_black_number;
		//д��ȥ
		disk_write(datablock_find_disk_id(now_file_node.indirect_pointer[index]), buffer_number);
	}
	return now_file_node;
}
/**///Ŀǰ�Ѿ�д���ˣ�����read_writeû�е���
/**///�����buffer_node�е����һ���ǡ�\n�����ţ�����ϵͳ�Լ���ӵ�
/**///!!!!!!!!!!!!!Ŀǰ����������Ȼд���˿������û�н��п��inode�����ӣ������޷���ʾ����һ����Ҫ�޸�
/**///�����Ѿ��޸ĺ���
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
	//��ȡinode
	int iNode_number = understand_the_path(path);
	iNode new_file_inode = just_read_inode_from_disk(iNode_number);
	printf(" in writing : the inode number is %d\n", iNode_number);
	//ȷ����ʼ��λ��
	int pre_number_size = offset / disk_char_amount;
	int start_position = offset % disk_char_amount;
	int have_wrote_size = 0;
	int disk_rank = pre_number_size;
	if (start_position == 0)//�ȹ���
	{
		//�����¿ռ�
		int tem_number = find_empty_datablock();
		make_data_map_covered(tem_number);
		//������
		new_file_inode = mount_block_to_inode_according_to_rank(pre_number_size,tem_number, new_file_inode);
	}

	/*�Ƚ��е�һ����д*/
	/**///��֮�Ȱѵ�һ��������
	char first_wirte_in[4096];
	read_the_one_disk_according_to_rank_in_inode(pre_number_size, new_file_inode,first_wirte_in);
	/**///���ڵ�һ�����̽���д����
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
		//�ܵĴ�С���ܳ���
		if (have_wrote_size >= ((int)(size) -1))
			break;
		//�����ں������д
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
	/**///ѭ��д��
	while (have_wrote_size<(int)size)
	{
		char  worte[4096] = { 0 };
		/**///���չ��д��ȥ
		for (int i = 0; i < disk_char_amount; i++)
		{
			if (have_wrote_size <= size)
				worte[i] = buffer[have_wrote_size];
			else
				break;
			have_wrote_size++;
		}
		/**///������д��ȥ
		/**///д֮ǰ�Ƚ��й���
		int tem_number = find_empty_datablock();
		make_data_map_covered(tem_number);
		new_file_inode =   mount_block_to_inode_according_to_rank(disk_rank, tem_number, new_file_inode);
		//����֮��д��
		write_into_disk_acccording_to_rank_in_inode(disk_rank, new_file_inode, worte);
		disk_rank++;

	}
	new_file_inode.st_size = (int)offset + (int)size;
	/**///�޸����iNode
	repalce_iNode_according_to_disk_number(iNode_number, new_file_inode);
	/**///�޸�����ļ��Ĵ�С
	printf("start checking write !!!!!!!!!!!!!!\n");
	check_write(path);
	printf("end of checking!!!!!!!!!!!!!!!!!\n");

	printf("Write is called:%s\n",path);
	return (int)size;
}

/**/
/**///ʹ��rank˼·������ɣ��ȴ�����
iNode delete_n_rank_block(int rank,iNode now_file_inode)
{
	int free_data_block_number;
	if (rank < 12)
		free_data_block_number = now_file_inode.direct_pointer[rank];
	else
	{
		int index = rank / (12 + 1024);
		//���bias
		int transfer = rank - 12 - 1024 * index;
		int tem_data_block_number[1024];
		//��ȡ����
		disk_read(datablock_find_disk_id(now_file_inode.indirect_pointer[index]), tem_data_block_number);
		//����datablock��number
		free_data_block_number = tem_data_block_number[transfer];

	}
	//����ɾ��
	make_data_map_uncovered(free_data_block_number);
	return now_file_inode;
}
iNode add_new_empty_block_according_to_rank(int rank , iNode now_file_number)
{
	if (rank < 12)
	{
		//���벢�ұ��
		int data_block_number = find_empty_datablock();
		make_data_map_covered(data_block_number);
		//����inode�Ķ�Ӧ
		now_file_number.direct_pointer[rank] = data_block_number;
	}
	else
	{
		//����Ԥ�ȵķ���
		if (rank == 12)
		{
			//���ڼ�ӵ�ָ�����ռ�
			int data_block_number = find_empty_datablock();
			make_data_map_covered(data_block_number);
			now_file_number.indirect_pointer[0] = data_block_number;
		}
		if (rank == (12 + 1024))
		{
			//���к�����ͬ���Ĳ���
			int data_block_number = find_empty_datablock();
			make_data_map_covered(data_block_number);
			now_file_number.indirect_pointer[1] = data_block_number;
		}
		//����д����
		int index = rank / (1024 + 12);
		int bias = rank - index * 1024 - 12;
		int buffer_index[1024];//�ȶ�ȡ����
		disk_read(datablock_find_disk_id(now_file_number.indirect_pointer[index]), buffer_index);
		//���ڶ������Ķ�����������
		/**///����ռ�
		int data_block_number = find_empty_datablock();
		make_data_map_covered(data_block_number);
		/**///���б��
		buffer_index[bias] = data_block_number;
		//д��ȥ
		disk_write(datablock_find_disk_id(now_file_number.indirect_pointer[index]), buffer_index);
	}
	//д��ȥ
	return now_file_number;
}
int fs_truncate (const char *path, off_t size)
{
	//����·��
	int inode_number = understand_the_path(path);
	//��ȡ�ļ���ϸ����Ϣ
	int disk_id; int index;
	inode_find_disk_id_and_index_in_disk(inode_number, &index, &disk_id);
	iNode buffer_inode[32];
	disk_read(disk_id, buffer_inode);
	off_t pre_size = buffer_inode[index].st_size;
	int pre_block_size = (int)pre_size / 4096 +1;
	int now_block_size = (int)size / 4096 + 1;
	//����Ҫ��datablock���д���
	if (pre_block_size == now_block_size)
		buffer_inode[index].st_size = size;
	else//��Ҫ���д���
	{
		if (now_block_size > pre_block_size)//��Ҫ�����������
		{
			for (int i = now_block_size + 1; i <= pre_block_size; i++)
				buffer_inode[index] = add_new_empty_block_according_to_rank(i, buffer_inode[index]);
			/*
				pre block ��������� 0.1.2
				now block �����������0��1��2
				���С�pre,next�� 0,0; 0,1;0,2;1,1;1,2;2,2,�������
			*/
		}
		else//��Ҫ����ɾ������
		{
			for (int i = (now_block_size + 1); i <= pre_block_size; i++)
				buffer_inode[index] = delete_n_rank_block(i, buffer_inode[index]);
			/*
				pre block ��������� 0.1.2
				now block �����������0��1��2
				���С�pre,next�� 0��0 �� 1��0��1��1�� 2��1��2��0��2��2���������
			*/
		}
	}
	//����size���иı�
	buffer_inode[index].st_size = size;
	//д��ȥ
	disk_write(disk_id, buffer_inode);
	printf("Truncate is called:%s\n",path);
	return 0;
}
/**///��touch��ʱ����ã���ɶȽϸ�
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
	//��data block �ı�־λȫ��������
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
	//һЩ�������
	stat->f_bsize = 4096;//��Ĵ�С
	stat->f_blocks = 256 * 256;//�������
	stat->f_namemax = 24;
	//��Ҫ����ɨ�����
	/**///�Ⱦ����ݿ�ĽǶȶ���
	stat->f_bfree = get_free_block_size();
	stat->f_bavail = get_free_block_size();
	/**///�پ��ļ�����
	stat->f_files = get_file_number();
	stat->f_ffree = 4096 - get_file_number();
	stat->f_favail = 4096 - get_file_number();
	printf("Statfs is called:%s\n",path);
	return 0;
}


/**///���ļ�������˳´���֮�������ߵĲ���Ҫ���κ�����
int fs_open (const char *path, struct fuse_file_info *fi)
{
	int iNode_number = understand_the_path(path);
	iNode file_inode  = just_read_inode_from_disk(iNode_number);
	/*
	������Ŀ�����ж�O_APPEND������fi->fh �м�¼��Ӧ����Ϣ
	��������ϵͳ�Լ����˶�Ӧ�Ĵ���
	������ʱ����ҪҪ���κ���
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
