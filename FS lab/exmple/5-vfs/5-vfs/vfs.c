#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BLOCKSIZE 1024
#define SIZE 1024 * 1024//1 M
#define END 65535 //EOF
#define FREE 48
#define USED 49
#define FCBBLOCKNUM 100
#define MAXLENGTH 10
#define MAXOPEN 5
#define COMMANDNUM 14

#define ERROR_DISK_NO_SPACE                                     99

#define ERROR_MKDIR_IS_FULL 			100
#define ERROR_MKDIR_CONFLICT_NAME 		101
#define ERROR_MKDIR_USELESS_DIRNAME 		102
#define ERROR_MKFILE_IS_FULL 			103
#define ERROR_CD_PATH_NOT_FOUND			104
#define ERROR_RM_FILE_NOT_FOUND			105
#define ERROR_RM_DIR_IS_NOT_EMPTY		106
//#define ERROR_WRITE_FILE_NOT_FOUND		107
#define ERROR_READ_FILE_NOT_FOUND		108
#define ERROR_READ_FILE_IS_EMPTY			109
#define ERROR_WRITE_FILE_NOT_FOUND		110
#define ERROR_OPEN_FILE_NOT_FOUND		111
#define ERROR_OPEN_FILE_IS_OPENED		112
#define ERROR_OPEN_OPENTABLE_IS_FULL                    113
#define ERROR_CLOSE_FILE_NOT_FOUND		114
#define ERROR_CLOSE_FILE_NOT_OPENED		115
#define ERROR_CP_SRCFILE_NOT_FOUND                      116
#define ERROR_CP_DSTFILE_IS_EXIST                              117
#define ERROR_CP_IS_FULL                                              118

typedef struct _FCB{
	int id;
	//int pid;//each dir has a parent id
	char filename[8];
	char isdir;//1:yes 2 no
	struct tm data_time;
	unsigned int firstblock;
	unsigned int size;
	int record[10]; //10 file in one dir most or 10 blocks in one file
	int length;
}FCB;

typedef struct _OPENTABLE{
	char name[20];
	struct tm data_time;
	int size;
}OPENTABLE;

typedef struct _FILEOPEN{
	OPENTABLE opened[MAXOPEN];
	int opennum;
}FILEOPEN;

#define MAXFILENUM FCBBLOCKNUM * BLOCKSIZE * sizeof(char) / sizeof(FCB)
FCB *root;
FCB *cur_dir;
FILEOPEN fileopened;
FILE *fp;
int id = 1;
int fd = -1;
char currentdir[80];
int currentid = 0;
char *fdisk;
time_t timep;
int GetCommandType(char *command);

void LoadSys();
void InitSystem();
//void mShowMenu();
void mFormat();
int mMkdir();
void mLs(void);
int mCd(void);
int mMkfile(void);
int mOpen(void);
int mClose(void);
int mRead(void);
int mWrite(void);
int mCp(void);
int mRm(void);
void mExit(void);
void ErrorConfig(int errorcode);
void FreeFCB(FCB *fcb);

int BackUp(void);





//0	command not exist
//1	format
//2	mkdir
//3	ls
//4	cd
//5	mkfile
//6	open
//7	close
//8	read
//9	write
//10	cp
//11	rm
//12	exit
int GetCommandType(char *command)
{
	char cmd[COMMANDNUM][10] = {
		"", "format", "mkdir", "ls", "cd",           //0:"", 1:format, 2:mkdir, 3:ls, 4:cd
		"mkfile", "open", "close", "read",         //5:mkfile, 6:open, 7:close, 8:read
		"write", "cp", "rm", "exit", "backup"                     //9:write, 10:cp, 11:rm, 12:exit
	};
	int i;
	for(i = 0; i < COMMANDNUM; i++)
	{
		if(strcmp(cmd[i], command) == 0)
			return i;
	}
	return 0;
}

void LoadSys()
{
	fileopened.opennum = 0;
	if((fp = fopen("VirtualDisk.txt", "rb")) == NULL)
	{
		printf("LoadSys : file doesn't exist!\n");
		//getchar();
		//fclose(fp);
		InitSystem(); // file doesn't exist
		return ;
	}
	printf("in load:");
	//getchar();
	fdisk = (char *)malloc(SIZE * sizeof(char));
	cur_dir = (FCB *)(fdisk + BLOCKSIZE + BLOCKSIZE);
	root = cur_dir;
	//cur_dir = fcb;

	//fseek(fp, 0, SEEK_SET);
	fread(fdisk, BLOCKSIZE, 1024, fp);
	fclose(fp);

	// int i;
	// for(i = 0; i < 6 * BLOCKSIZE; i++)
	// {
	// 	printf("%c ", fdisk[i]);
	// 	if(i % 1024 == 0)
	// 		printf("\n\n\n");
	// }
	// getchar();

	if(fdisk[0] != '$')
	{
		printf("%c doesn't find the disk!\n", fdisk[0]);
		mFormat();
	}

	return;
}

//initial the system
void InitSystem()
{
	int i;
	fdisk = (char *)malloc(SIZE * sizeof(char));

	printf("sizeof(fdisk) = %d\n", (int)sizeof(fdisk));
	//getchar();
	mFormat();
}

void mFormat()
{
	fp = fopen("VirtualDisk.txt", "wb+");
	int i;

	//record of 
	char *p = fdisk + BLOCKSIZE;

	for(i = 0; i < SIZE; i++)
		fdisk[i] = '_';
	fdisk[0] = '$';

	for(i = 0; i < 2 + FCBBLOCKNUM; i++)
		p[i] = USED;
	for(; i < BLOCKSIZE; i++)
		p[i] = FREE;
	printf("\n\n\nUSEINFORMATION\n");
	for(i = 0; i < BLOCKSIZE; i++)
		printf("%d", p[i]);
	printf("\n\n\n");

	FCB *fcb = (FCB *)(fdisk + BLOCKSIZE + BLOCKSIZE);
	cur_dir = fcb;
	root = cur_dir;
	timep = time(NULL);
	for(i = 0; i < MAXFILENUM; i++)
	{
		fcb[i].id = -1;
		strcpy(fcb[i].filename, "");
		fcb[i].isdir = 0;
		fcb[i].data_time = *localtime(&timep);
		fcb[i].firstblock = 0;
		fcb[i].size = 0;
		fcb[i].length = 0;
		int j;
		for(j = 0; j < 10; j++)
			fcb[i].record[j] = -1;
		printf("set %2d: %2d, %s\n", i, fcb[i].id, fcb[i].filename);
	}

	fcb[0].id = 0;
	strcpy(fcb[0].filename, "\\");//"home"
	fcb[0].isdir = 1;
	fcb[0].data_time = *localtime(&timep);
	fcb[0].firstblock = 0; 
	fcb[0].size = 0;
	fcb[0].length = 2;
	fcb[0].record[0] = 1;  //"."
	fcb[0].record[1] = 2; //".."


	fcb[1].id = 1;
	strcpy(fcb[1].filename, ".");
	fcb[1].isdir = 1;
	fcb[1].data_time = *localtime(&timep);
	fcb[1].firstblock = 0; 
	fcb[1].size = 0;
	fcb[1].length = 1;
	fcb[1].record[0] = 0; //"home"

	fcb[2].id = 2;
	strcpy(fcb[2].filename, "..");
	fcb[2].isdir = 1;
	fcb[2].data_time = *localtime(&timep);
	fcb[2].firstblock = 0; 
	fcb[2].size = 0;
	fcb[2].length = 1;
	fcb[2].record[0] = 0;//"home"

	printf("sizeof(char) = %d, sizeof(FCB) = %d, MAXFILENUM = %d\n", (int)sizeof(char), (int)sizeof(FCB), (int)(MAXFILENUM));

	for(i = 0; i < MAXFILENUM; i++)
	{
		printf("hehe");
		//getchar();
		printf("i = %2d, %d", i, fcb[i].id);
		if(fcb[i].id != -1)
		{
			printf("%s\n", fcb[i].filename);
		}
	}
	//printf("h");
	sleep(1);
	//getchar();
	for(i = 0; i < SIZE; i++)
	{
		//if(fdisk[i] == USED)
		//	printf("1");
		//if(fdisk[i] == FREE)
		//	printf("0");
		if(i % BLOCKSIZE == 0)
			printf("\n\n\n\n");
		printf("%c",fdisk[i]);
		//getchar();
		//fputc(fdisk[i], fp);

	}

	fwrite(fdisk, BLOCKSIZE, 1024, fp);
	fseek(fp, 0, SEEK_SET);

	return;
}

int mMkdir()
{
	fp = fopen("VirtualDisk.txt", "wb+");
	printf("in mkdir\n");
	int i, a, b, j;
	char dirname[20];
	scanf("%s", dirname);

	FCB *fcb = (FCB *)(fdisk + BLOCKSIZE + BLOCKSIZE);
	char *flag = (char *)(fdisk + BLOCKSIZE);
	FCB *cur = cur_dir;
	FCB *temp;
	if(cur->length == MAXLENGTH)
		return ERROR_MKDIR_IS_FULL;


	//find if the name of dir is exist
	for (i = 0; i < cur->length; i++)
	{
		temp = fcb + cur->length;
		if(strcmp(temp->filename, dirname) == 0)          //    !!!!!!!!!!!
			return ERROR_MKDIR_CONFLICT_NAME;
	}

	for(i = 0; i < MAXFILENUM; i++)
	{
		if(fcb[i].id == -1)
			break;
	}
	timep = time(NULL);
	fcb[i].id = i;
	strcpy(fcb[i].filename, dirname);
	fcb[i].isdir = 1;
	fcb[i].data_time = *localtime(&timep);
	fcb[i].firstblock = 0;
	fcb[i].size = 0;
	fcb[i].length = 2;

	//todo :record[0] record[1];

	//sub dir of dirname

	for(a = i; a < MAXFILENUM; a++)
	{
		if(fcb[a].id == -1)
			break;
	}

	for(j = 2; j < 10; j++)
		fcb[i].record[j] = -1;

	fcb[a].id = a;
	strcpy(fcb[a].filename, ".");
	fcb[a].isdir = 1;
	fcb[a].data_time = *localtime(&timep);
	fcb[a].firstblock = 0;
	fcb[a].size = 0;
	fcb[a].length = 1;
	fcb[a].record[0] = i;
	for(j = 1; j < 10; j++)
		fcb[a].record[j] = -1;

	for(b = a; b < MAXFILENUM; b++)
	{
		if(fcb[b].id == -1)
			break;
	}
	fcb[b].id = b;
	strcpy(fcb[b].filename, "..");
	fcb[b].isdir = 1;
	fcb[b].data_time = *localtime(&timep);
	fcb[b].firstblock = 0;
	fcb[b].size = 0;
	fcb[b].length = 1;
	fcb[b].record[0] = cur->id;
	for(j = 1; j < 10; j++)
		fcb[b].record[j] = -1;

	fcb[i].record[0] = a;
	fcb[i].record[1] = b;

	printf("max = %d, i = %d, a = %d, b = %d, cur-id = %d\n",(int)(MAXFILENUM), i, a, b, cur->id);


	cur->record[cur->length] = i;
	cur->length = cur->length + 1;
	cur->data_time = *localtime(&timep);

	fwrite(fdisk, BLOCKSIZE, 1024, fp);
	fclose(fp);
	return 1;
}

void mLs()
{
	FCB *fcb = (FCB *)(fdisk + BLOCKSIZE + BLOCKSIZE);
	FCB temp;

	//printf("in ls, curdir:%s\n", cur_dir->filename);
	int i;
	for(i = 0; i < 10; i++)
	{
		int pos = cur_dir->record[i];
		if(pos >= 0)
		{
			temp = fcb[pos];
			if(temp.isdir)
			{
				printf("%d ", pos);
				printf("%s\t\t\t\t\t", temp.filename);
				printf ("%d %d %d ", (1900 + temp.data_time.tm_year), (1 + temp.data_time.tm_mon), temp.data_time.tm_mday);
				printf("%d:%d:%d\n", temp.data_time.tm_hour, temp.data_time.tm_min, temp.data_time.tm_sec);
			}
			else
			{
				printf("%d ", pos);
				printf("%s\t\t%d\t\t", temp.filename, temp.size);
				printf ("%d %d %d ", (1900 + temp.data_time.tm_year), (1 + temp.data_time.tm_mon), temp.data_time.tm_mday);
				printf("%d:%d:%d\n", temp.data_time.tm_hour, temp.data_time.tm_min, temp.data_time.tm_sec);
			}
		}
	}

	return;
}
int mCd()
{
	printf("in mcd!\n");
	int i;
	char dir[20];
	scanf("%s", dir);

	FCB *fcb = (FCB *)(fdisk + BLOCKSIZE + BLOCKSIZE);
	char *flag = (char *)(fdisk + BLOCKSIZE);
	FCB *cur = cur_dir;
	FCB *temp;
	if(strcmp(dir, "\\") == 0)
	{
		cur_dir = root;
		return 1;
	}
	if(strcmp(dir, ".") == 0)
		return 1;
	else if(strcmp(dir, "..") == 0)
	{
		printf("found %d\t", cur->record[i]);
		temp = fcb + cur->record[1];
		cur_dir = fcb + temp->record[0];
		printf("found : %d", temp->record[0]);
		return 1;
	}


	for(i = 2; i < 10; i++)
	{
		temp = fcb + cur->record[i];
		if(strcmp(temp->filename, dir) == 0)
		{
			printf("found : %d, %s\n", temp->id,temp->filename);
			break;
		}
	}
	if(i == 10 || temp->isdir == 0)
		return ERROR_CD_PATH_NOT_FOUND;
	cur_dir = temp;


	return 1;
}
int mMkfile()
{
	fp = fopen("VirtualDisk.txt", "wb+");
	printf("in mkfile\n");
	int i;
	char filename[20];
	scanf("%s", filename);

	FCB *fcb = (FCB *)(fdisk + BLOCKSIZE + BLOCKSIZE);
	char *flag = (char *)(fdisk + BLOCKSIZE);
	FCB *cur = cur_dir;

	if(cur->length == MAXLENGTH)
		return ERROR_MKFILE_IS_FULL;


	for(i = 0; i < MAXFILENUM; i++)
	{
		if(fcb[i].id == -1)
			break;
	}
	if(i == MAXFILENUM)
		return ERROR_DISK_NO_SPACE;

	timep = time(NULL);

	fcb[i].id = i;
	strcpy(fcb[i].filename, filename);
	fcb[i].isdir = 0;
	fcb[i].data_time = *localtime(&timep);
	fcb[i].firstblock = 0;
	fcb[i].size = 0;
	fcb[i].length = 0;

	cur->record[cur->length] = i;
	cur->length = cur->length + 1;
	cur->data_time = *localtime(&timep);

	fwrite(fdisk, BLOCKSIZE, 20, fp);
	fclose(fp);
	return 1;
}
int mOpen()
{
	int i;
	FCB *fcb = (FCB *)(fdisk + BLOCKSIZE + BLOCKSIZE);
	char *flag = (char *)(fdisk + BLOCKSIZE);
	FCB *cur = cur_dir;
	FCB *temp;

	char filename[20];
	scanf("%s", filename);

	for(i = 0; i < 10; i++)
	{
		if(cur->record[i] != -1)
		{
			temp = fcb + cur->record[i];
			if(strcmp(temp->filename, filename) == 0)
			{
				break;
			}
		}
	}
	if(temp->isdir == 1 || i == 10)
		return ERROR_OPEN_FILE_NOT_FOUND;

	if(fileopened.opennum == MAXOPEN)
		return ERROR_OPEN_OPENTABLE_IS_FULL;

	for(i = 0; i < fileopened.opennum; i++)
	{
		printf("search : %s\n", fileopened.opened[i].name);
		if(strcmp(fileopened.opened[i].name, filename) == 0)
		{
			return ERROR_OPEN_FILE_IS_OPENED;
		}
	}

	timep = time(NULL);
	OPENTABLE *openitem = &fileopened.opened[fileopened.opennum];
	fileopened.opennum++;
	strcpy(openitem->name, filename);
	openitem->data_time  = *localtime(&timep);
	temp->data_time = openitem->data_time;
	openitem->size = temp->size;
	printf("opened %d\n", fileopened.opennum);
	printf("Just opened:%s\n", fileopened.opened[fileopened.opennum - 1].name);

	/*
	printf("1:%d\n", fileopened.opennum);
	printf("2:%s\n", filename);
	strcpy(fileopened.opened[fileopened.opennum].name, filename);
	printf("3:%s\n", fileopened.opened[fileopened.opennum].name);
	fileopened.opened[fileopened.opennum].data_time = *localtime(&timep);
	fileopened.opened[fileopened.opennum].size = temp->size;
	printf("opened :%d now\n", fileopened.opennum);
	printf("just opened:%s\n", fileopened.opened[fileopened.opennum].name);
	fileopened.opennum++;
	*/


	return 1;
}
int mClose()
{
	int i;
	char filename[20];
	scanf("%s", filename);
	int opennum = fileopened.opennum;

	for(i = 0; i < opennum; i++)
	{
		if(strcmp(fileopened.opened[i].name, filename) == 0)
			break;
	}
	if(i == opennum)
		return ERROR_CLOSE_FILE_NOT_OPENED;

	printf("find !\n");
	for( ; i < opennum - 1; i++)
	{
		fileopened.opened[i] = fileopened.opened[i + 1];
	}
	fileopened.opennum--;
	//delete opened file from fileopentable

	return 1;
}
int mRead()
{
	printf("in mRed!\n");
	int i, j;
	FCB *fcb = (FCB *)(fdisk + BLOCKSIZE + BLOCKSIZE);
	char *flag = (char *)(fdisk + BLOCKSIZE);
	FCB *cur = cur_dir;
	FCB *temp;

	char filename[20];
	scanf("%s", filename);

	for(i = 0; i < 10; i++)
	{
		if(cur->record[i] != -1)
		{
			temp = fcb + cur->record[i];
			if(strcmp(temp->filename, filename) == 0)
			{
				break;
			}
		}
	}
	if(temp->isdir == 1 || i == 10)
		return ERROR_READ_FILE_NOT_FOUND;

	printf("Reading file: %s\n", temp->filename);

	int block[10], blocknum = 0, sizeread = 0, blockread = 0, readingblock;
	//temp is the file to read
	blocknum = temp->length;
	//record the file's block-id
	for(i = 0; i < blocknum; i++)
	{
		block[i] = temp->record[i];
		printf("block[i] = %d\n", block[i]);
	}

	//read file
	char *reading;
	printf("\t\t\t\t\tNow reading!\n");
	while(sizeread < temp->size)
	{
		readingblock = sizeread / BLOCKSIZE;
		reading = fdisk + 1024 * block[readingblock] + sizeread % BLOCKSIZE;    //array to the char that is reading
		//printf("readingblock = %d, sizeread = %d,  reading = %d, read: %c\n", readingblock, sizeread,1024 * block[readingblock] + sizeread % BLOCKSIZE, *reading);
		printf("%c", *reading);
		sizeread++;
	}
	printf("\n\nRead %d chars\n", sizeread);

	return 1;
}
int mWrite()
{
	fp = fopen("VirtualDisk.txt", "wb+");

	printf("in mWrite\n");
	int i, j;
	char filename[20];
	scanf("%s", filename);

	FCB *fcb = (FCB *)(fdisk + BLOCKSIZE + BLOCKSIZE);
	char *flag = (char *)(fdisk + BLOCKSIZE);
	FCB *cur = cur_dir;
	FCB *temp;

	for(i = 0; i < 10; i++)
	{
		if(cur->record[i] != -1)
		{
			temp = fcb + cur->record[i];
			if(strcmp(temp->filename, filename) == 0)
			{
				break;
			}
		}
	}
	if(temp->isdir == 1 || i == 10)
		return ERROR_WRITE_FILE_NOT_FOUND;

	//foundfile: temp
	printf("please enter you file, end with '#'\n");
	char ch;
	char blockcontent[10][BLOCKSIZE];
	for(i = 0; i < 10; i++)
	{
		for(j = 0; j < BLOCKSIZE; j++)
			blockcontent[i][j] = '\0';
	}

	int count = 0, blocknum = 0;
	getchar();
	while((ch = getchar()) != '#')
	{
		blockcontent[blocknum][count] = ch;
		count++;
		if(count >= BLOCKSIZE)
		{
			blocknum++;
			count = count - BLOCKSIZE;
		}
	}
	blocknum++;
	
	int block[10] = {-1};
	int findnum = 0;
	//find count block's space that can write file
	printf("blocknum = %d\n", blocknum);
	//getchar();
	for(i = 0; i < BLOCKSIZE; i++)
	{
		if(flag[i] == FREE)
		{
			flag[i] = USED;
			printf("find block :%d free, findnum = %d\n", i, findnum);
			block[findnum] = i;
			findnum++;
			if(findnum == blocknum)
				break;
		}
	}

	printf("here is block array!");
	for(i = 0; i < blocknum;i++)
	{
		printf("%d ", block[i]);
	}

	int overflag = 0, writenum = 0;;
	for(i = 0; i < blocknum; i++)
	{
		int k = block[i];
		for(j = 0; j < BLOCKSIZE; j++)
		{
			printf("write to %d, %c\n", 1024 * k + j, blockcontent[i][j]);
			fdisk[1024 * k+j] = blockcontent[i][j];
			writenum++;
			if(writenum == count)
			{
				overflag = 1;
				break;
			}
		}
		if(overflag == 1)
			break;
	}

	timep = time(NULL);

	printf("write id : %d, blocknum = %d\n", temp->id, blocknum);
	temp->size = count;
	temp->data_time = *localtime(&timep);
	printf("count = %d, size = %d\n", count, temp->size);
	for(i = 0; i < blocknum; i++)
	{
		temp->record[i] = block[i];
		printf("to block %d\n", temp->record[i]);
	}
	for(i = blocknum; i < 10; i++)
		temp->record[i] = -1;
	temp->length = blocknum;
	fwrite(fdisk, BLOCKSIZE, 1024, fp);
	fclose(fp);

	return 1;
}

int mCp()
{
	printf("in mCp\n");
	int i,j, fcbid;
	char srcfile[20], dstfile[20];

	FCB *fcb = (FCB *)(fdisk + BLOCKSIZE + BLOCKSIZE);
	char *flag = (char *)(fdisk + BLOCKSIZE);
	FCB *cur = cur_dir;
	FCB *tempsrc, *tempdst;
	scanf("%s %s", srcfile, dstfile);
	for(i = 0; i < 10; i++)
	{
		if(cur->record[i] != -1)
		{
			tempsrc = fcb + cur->record[i];
			if(strcmp(tempsrc->filename, srcfile) == 0)
			{
				break;
			}
		}
	}
	if(tempsrc->isdir == 1 || i == 10)
		return ERROR_CP_SRCFILE_NOT_FOUND;

	for(i = 0; i < 10; i++)
	{
		if(cur->record[i] != -1)
		{
			tempdst = fcb + cur->record[i];
			if(strcmp(tempdst->filename, dstfile) == 0)
			{
				break;
			}
		}
	}
	if(i != 10)//didnot think about dir is full
		return ERROR_CP_DSTFILE_IS_EXIST;

	for(i = 0; i < MAXFILENUM; i++)
	{
		if(fcb[i].id == -1)
			break;
	}
	if(i == MAXFILENUM)
		return ERROR_DISK_NO_SPACE;
	fcbid = i;
	tempdst = fcb + i;
	printf("srcfile found\n");

	int blocknum = tempsrc->length;
	int findnum = 0;
	int block[10] = {0};
	//find block that is free and store the blocks to block[10]

	if(blocknum == 0)
	{
		timep = time(NULL);
		tempdst->id = fcbid;
		strcpy(tempdst->filename, dstfile);
		tempdst->isdir = 0;
		tempdst->data_time = *localtime(&timep);
		tempdst->firstblock = 0;
		tempdst->size = tempsrc->size;
		tempdst->length = tempsrc->length;

		//change the info of dir
		cur->record[cur->length] = fcbid;
		cur->length = cur->length + 1;
		cur->data_time = *localtime(&timep);
		return 1;
	}

	for(i = 0; i < BLOCKSIZE; i++)
	{
		if(flag[i] == FREE)
		{
			flag[i] = USED;
			printf("findblock%d, findnum = %d", i, findnum+1);
			block[findnum] = i;
			findnum++;
			if(findnum == blocknum)
				break;
		}
	}

	printf("find fcb id :%d\n", fcbid);
	//set fcb info
	timep = time(NULL);
	tempdst->id = fcbid;
	strcpy(tempdst->filename, dstfile);
	tempdst->isdir = 0;
	tempdst->data_time = *localtime(&timep);
	tempdst->firstblock = 0;
	tempdst->size = tempsrc->size;
	tempdst->length = tempsrc->length;

	//change the info of dir
	cur->record[cur->length] = fcbid;
	cur->length = cur->length + 1;
	cur->data_time = *localtime(&timep);

	for(i = 0; i < tempdst->length; i++)
	{
		tempdst->record[i] = block[i];
	}

	//copying file
	char *reading;
	char blockcontent[10][BLOCKSIZE];
	int copycount = 0, readingblock;
	int count = tempsrc->size;

	//read
	while(copycount < count)
	{
		readingblock = copycount / BLOCKSIZE;
		reading = fdisk + 1024 * tempsrc->record[readingblock] + copycount % BLOCKSIZE;    //array to the char that is reading
		printf("read %c\n", *reading);
		blockcontent[readingblock][copycount % BLOCKSIZE] = *reading;
		copycount++;
	}

	//write
	int overflag = 0, writenum = 0;
	for(i = 0; i < blocknum; i++)
	{
		int k = block[i];
		for(j = 0; j < BLOCKSIZE; j++)
		{
			fdisk[1024 * k + j] = blockcontent[i][j];
			writenum++;
			if(writenum == count)
			{
				overflag = 1;
				break;
			}
		}
		if(overflag = 1)
			break;
	}



	fopen("VirtualDisk.txt", "wb+");
	fwrite(fdisk, BLOCKSIZE, 1024, fp);
	fclose(fp);

	return 1;
}

int mRm()
{
	fp = fopen("VirtualDisk.txt", "wb+");
	printf("in mRm\n");
	int i;
	char filename[20];
	scanf("%s", filename);

	FCB *fcb = (FCB *)(fdisk + BLOCKSIZE + BLOCKSIZE);
	char *flag = (char *)(fdisk + BLOCKSIZE);
	FCB *cur = cur_dir;
	FCB *temp;

	for(i = 0; i < 10; i++)
	{
		temp = fcb + cur->record[i];
		if(strcmp(temp->filename, filename) == 0)
		{
			if(temp->isdir == 1 && temp->length > 2)
			{
				printf("dir not free\n");
				return ERROR_RM_DIR_IS_NOT_EMPTY;
			}
			else if(temp->isdir == 1 && temp->length == 2)
			{
				//delete the dir
				printf("%d, dir, free!\n", i);
				FreeFCB(fcb + temp->record[0]);
				FreeFCB(fcb + temp->record[1]);
				FreeFCB(temp);

				cur->length = cur->length - 1;
				cur->record[i] = -1;
				fwrite(fdisk, BLOCKSIZE, 1024, fp);
				fclose(fp);
				return 1;
			}
			else
			{
				printf("file");
				int j;
				for(j = 0; j < 10; j++)
				{
					int blocknum = temp->record[j];
					printf("find block %d\n", blocknum);
					if(blocknum != -1)
					{
						flag[blocknum] = FREE;
					}
				}

				//delete the file
				FreeFCB(temp);
				cur->length = cur->length - 1;
				cur->record[i] = -1;
				fwrite(fdisk, BLOCKSIZE, 1024, fp);
				fclose(fp);
				return 1;

			}
		}
	}
	if(i == 10)
		return ERROR_CD_PATH_NOT_FOUND;
	cur_dir = temp;
	return;
}
void mExit()
{
	printf("GoodBye!\n");
	exit(0);
	//return;
}



void main(void)
{
	int ordertype, error;
	char command[10];
	LoadSys();
	while(1)
	{
		printf(">:");
		scanf("%s", command);
		ordertype = GetCommandType(command);
		//printf("%d\n", ordertype);
		switch (ordertype)
		{
			//case 0:
			//	break;
			case 1://format
				mFormat();
				break;
			case 2://mkdir
				error = mMkdir();
				ErrorConfig(error);
				break;
			case 3://ls
				mLs();
				break;
			case 4://cd
				error = mCd();
				ErrorConfig(error);
				break;
			case 5://mkfile
				error = mMkfile();
				ErrorConfig(error);
				break;
			case 6://open
				error = mOpen();
				ErrorConfig(error);
				break;
			case 7://close
				error = mClose();
				ErrorConfig(error);
				break;
			case 8://read
				error = mRead();
				ErrorConfig(error);
				break;
			case 9://write
				error = mWrite();
				ErrorConfig(error);
				break;
			case 10://cp
				error = mCp();
				ErrorConfig(error);
				break;
			case 11://rm
				error = mRm();
				ErrorConfig(error);
				break;
			case 12://exit
				mExit();
				break;
			case 13://todo
				BackUp();
				break;
			default:
				printf("order not found\n");
				break;
		}
	}
}

void FreeFCB(FCB *fcb)
{
	int i;
	printf("free fcb %d\n", fcb->id);
	fcb->id = -1;
	strcpy(fcb->filename, "");
	fcb->firstblock = 0;
	fcb->size = 0;
	fcb->length = 0;
	for(i = 0; i < 10; i++)
		fcb->record[i] = -1;
}

void ErrorConfig(int errorcode)
{
	switch(errorcode)
	{
		case ERROR_MKDIR_IS_FULL:
			printf("Error: the dir is full now, cannot create dir in it!\n");
			break;
		case ERROR_MKDIR_CONFLICT_NAME:
			printf("Error: dir exists\n");
			break;
		case ERROR_MKDIR_USELESS_DIRNAME:
			printf("Error: dir name is invalid\n");
			break;
		case ERROR_CD_PATH_NOT_FOUND:
			printf("Error: please enter the right path!\n");
			break;
		case ERROR_RM_FILE_NOT_FOUND:
			printf("Error: please enter the right filename!\n");
			break;
		case ERROR_RM_DIR_IS_NOT_EMPTY:
			printf("Error: cannot delete a directory with items!\n");
			break;
		case ERROR_READ_FILE_NOT_FOUND:
			printf("Error: cannot read, file is not found!\n");
			break;
		case ERROR_READ_FILE_IS_EMPTY:
			printf("Error: cannot read, file is empty!");
			break;
		case ERROR_WRITE_FILE_NOT_FOUND:
			printf("Error: cannot write, file is not found!\n");
			break;
		case ERROR_OPEN_FILE_NOT_FOUND:
			printf("Error: cannot open file, file not found!\n");
			break;
		case ERROR_OPEN_FILE_IS_OPENED:
			printf("Error: cannot open file, file is opened!\n");
			break;
		case ERROR_CLOSE_FILE_NOT_OPENED:
			printf("Error: cannot close file, file is not opened!\n");
			break;
		default:
			return;
	}
}

int BackUp(void)
{
	char s[50] = "cp VirtualDisk.txt backup.disk";
	system(s);
	return 1;
}
















