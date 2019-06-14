#define BLOCKSIZE 512
#define NUMBEROFBLOCKS 1000
#define FILESYSTEMNAME "SimpleFileSystem"
#define NUMBEROFINODES 100
#define MAXNAMELENGTH 25
#define MAXEXTENSIONLENGTH 3
#define MAXOPENFILES 100
#define MAGIC 83934122
#define MAXNUMBEROFFILES 100

//Booleans
#define TRUE 1
#define FALSE 0

//Usage
#define FREE 0
#define IN_USE 1

//Permissions
#define RWE 666
#define R 555
#define ROOT_UID 0
#define ROOT_GID 0
#define USER_UID 1
#define USER_GID 1

//#include <stdio.h>
//#include "log.c"
//#include "disk_emu.c" //Maybe need to comment?
#include "disk_emu.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

/* TODO:
- Implement support for persistence: Init disk not fresh
- Don't forget to initialize free block bit map on mkfs
- Debug readDir() and add writeDir()
- Debug FD table (error when try to open same file twice with fopen)
- All other methods. sfs seek and read and write
- Make sure file is closed in sfs remove
- Add indirect pointers to read


Flagged errors:
- in readDirectory()

*/

typedef struct {
	//main index
	int iNode;
	//Read/Write pointer
	int RWPointer;

}FileDescriptor;

typedef struct {
	long magic;
	long blockSize;
	long fileSystemSize;
	long iNodeTableLength;
	long rootINode;
	char unUsed[512 - 5*sizeof(long)];
} SuperBlock;

typedef struct {
	long mode;
	long linkCnt;
	long uid;
	long gid;
	long size;
	long directPointers[12];
	long indirectPointer;
}iNode;

typedef struct{
	long pointers[128];
} indirectINode;

typedef struct {
	char name[MAXNAMELENGTH+MAXEXTENSIONLENGTH];
	long iNode;
} DirectoryEntry;



SuperBlock mySuperBlock;
DirectoryEntry* directory; //root directory
int positionInDir=0;
FileDescriptor* fileDescTable;

void initializeFDTable(){
	fileDescTable = (FileDescriptor*)malloc(MAXOPENFILES*sizeof(FileDescriptor*));
}

int addToFDTable(int iNode){
	int index=0;
	while((fileDescTable[index].iNode!=0) && (index<MAXOPENFILES) && (fileDescTable[index].iNode!=iNode)){
		index++;
	}
	if(fileDescTable[index].iNode==0){
		fileDescTable[index].iNode = iNode;
		fileDescTable[index].RWPointer = 0;
		return index;
	}
	else if(fileDescTable[index].iNode==iNode){
		return index;
	}
	else{
		return -1;
	}
}

int removeFromFDTable(int fileID){
	if(fileDescTable[fileID].iNode == 0){
		return -1;
	}
	else {
		fileDescTable[fileID].iNode = 0;
		fileDescTable[fileID].RWPointer = 0;
		return 0;
	}

}

void initializeFreeBitMap(){
	//Compute the number of blocks required for the free block bit map:
	int blocksReq = (NUMBEROFBLOCKS+(BLOCKSIZE-1))/(BLOCKSIZE); //Adding blockSize to roundup.
	//We are going to write 1s in every bit
	unsigned char buffer[512];
	memset(buffer,FREE,512);
	//Compute start block:
	int currentBlock = NUMBEROFBLOCKS-blocksReq;

	for(currentBlock;currentBlock<NUMBEROFBLOCKS;currentBlock++){
		write_blocks(currentBlock,1,&buffer);
	}

	//Write the blocks for SB and I-Node table as occupied
	int numBlocksReqForINodes = (NUMBEROFINODES * sizeof(iNode) + (BLOCKSIZE-1))/BLOCKSIZE;
	int i;
	for(i=0;i<1+numBlocksReqForINodes;i++){
		changeBlockStatus(i,IN_USE);
	}

	//Write the blocks of the free bit map as occupied:
	for(i=NUMBEROFBLOCKS-1;i>=NUMBEROFBLOCKS-blocksReq;i--){
		changeBlockStatus(i,IN_USE);
	}
}

int addToDirectory(char* name, int iNode){
	//Find an empty spot in the directory
	int index=0;
	while((directory[index].iNode!=0) &&index<NUMBEROFINODES) {
		index++;
	}
	strcpy(directory[index].name , name);
	directory[index].iNode = iNode;

	return index;
}

int findDirectoryEntry(const char* name){
	//Find the file with the given name in the directory
	int index=0;
	while((strcmp(name,directory[index].name)!=0) && (index<NUMBEROFINODES)){
		index++;
	}
	if(strcmp(name,directory[index].name)==0){
		return index;
	}
	else{
		return -1;
	}
}

void clearDirectoryAtIndex(int index){
	strcpy(directory[index].name,"");
	directory[index].iNode = FREE;
}

void clearINode(int numI){
	unsigned char blockBuffer[512];
	//Reading the corresponding block from disk
	read_blocks(findINodeBlockAddress(numI),1,&blockBuffer);

	iNode fileINode;
	memcpy(&fileINode,(void*)(blockBuffer+findINodeByteAddress(numI)),sizeof(fileINode));

	fileINode.mode = 0;
	fileINode.linkCnt = 0;
	fileINode.uid = 0;
	fileINode.gid = 0;
	fileINode.size = 0;

	int i;
	for(i=0;i<12;i++){
		if(fileINode.directPointers[i]!=0){
			changeBlockStatus(fileINode.directPointers[i],FREE);
			fileINode.directPointers[i]=0;
		}
	}
	if(fileINode.indirectPointer!=0){
		//Read the indirect block
		read_blocks(fileINode.indirectPointer,1,&blockBuffer);
		indirectINode indirect;
		memcpy(&indirect,blockBuffer,BLOCKSIZE);

		for(i=0;i<BLOCKSIZE/4;i++){
			if(indirect.pointers[i]!=0){
				changeBlockStatus(indirect.pointers[i],FREE);
			}
		}

		//Zero out the indirect block
		unsigned char emptyBlock[512];
		memset(&emptyBlock,0,512);
		write_blocks(fileINode.indirectPointer,1,&emptyBlock);
		fileINode.indirectPointer=0;
	}
	//Make sure to clear and release memory blocks
	memcpy((void*)(blockBuffer+findINodeByteAddress(numI)),&fileINode,sizeof(fileINode));

	write_blocks(findINodeBlockAddress(numI),1,&blockBuffer);

}

void initDirectory(){
	directory = (DirectoryEntry*)malloc(NUMBEROFINODES*sizeof(DirectoryEntry));
	int i;
	for(i=0; i<NUMBEROFINODES;i++){
		directory[i].iNode=0;
		strcpy(directory[i].name,"               ");
	}
}

void readDirectory(){
	//retrieve iNode 0;
	unsigned char blockBuffer[512];
	read_blocks(1,1,&blockBuffer);
	iNode rootINode;
	memcpy(&rootINode,blockBuffer,sizeof(iNode));
	int numBlocksToRead = (rootINode.size+BLOCKSIZE-1)/BLOCKSIZE; //Rounded Up
	long blocksToGet[numBlocksToRead];
	//printf("Num blocks to get %d\n", numBlocksToRead);
	//printf("Root inode indirect: %ld\n", rootINode.indirectPointer);
	if(rootINode.indirectPointer != 0){

		unsigned char blockBufferIndirect[512];
		read_blocks(rootINode.indirectPointer,1,&blockBufferIndirect);      //FLAGGED check

		indirectINode indirect;
		memcpy(&indirect,blockBufferIndirect,sizeof(indirectINode));
		int i=0;
		while(indirect.pointers[i] != 0) {
			blocksToGet[i+12]=indirect.pointers[i];
			i++;
		}
	}
	int i=0;
	while(rootINode.directPointers[i]!=0){
		blocksToGet[i]=rootINode.directPointers[i];
		i++;
	}
	//printf("%s%d\n","Number of blocks to read: ",numBlocksToRead);
	unsigned char directoryBlocks[rootINode.size];//Create an array to hold the directory
	//int numBlocksLeftToRead = numBlocksToRead;
	int sizeLeftToRead = rootINode.size;

	for(i=0;i<numBlocksToRead;i++){
		//printf("%s%d\n","i: ",i);
		read_blocks(blocksToGet[i],1,&blockBuffer);						//FLAGGED check

		if(sizeLeftToRead>512) {
			memcpy((void*)(directoryBlocks+512*i),blockBuffer,512);
			sizeLeftToRead = sizeLeftToRead - 512;
		}
		else{
			memcpy((void*)(directoryBlocks+512*i),blockBuffer,sizeLeftToRead);
			sizeLeftToRead = 0;
		}
	}
	directory = (DirectoryEntry*) malloc(NUMBEROFINODES*sizeof(DirectoryEntry));
	memcpy(&directory,directoryBlocks,rootINode.size);
}


int findEmptyINode(){
	int i;
	unsigned char blockBuffer[512]; //To contain the current block
	int currentBlock = -1; //Initialized corectly in the following loop:
	for(i=0;i<NUMBEROFINODES;i++){
		if(currentBlock != findINodeBlockAddress(i)){
			currentBlock = findINodeBlockAddress(i);
			//Reading the corresponding block from disk
			read_blocks(currentBlock,1,&blockBuffer);
		}
		//Find the mode of the current iNode. If Mode=0, this iNode is unallocated
		long currentMode;
		memcpy(&currentMode,(void*)(blockBuffer+findINodeByteAddress(i)),4);
		if(currentMode==0){
			return i;
		}
		else{
			continue;
		}
	}
	return -1;
}

//Find the block in which the iNode is stored
int findINodeBlockAddress(int iNodeNumber){
	if(iNodeNumber>NUMBEROFINODES){
		printf("iNode doesn't exist");
		return -1;
	}
	int block = (iNodeNumber*sizeof(iNode))/BLOCKSIZE;
	return block+1;//Since iNodes start at block 1 and not block 0
}

//Find the iNode's start byte in the block
int findINodeByteAddress(int iNodeNumber){
	int block = (iNodeNumber*sizeof(iNode))/BLOCKSIZE;
	int byte = (iNodeNumber*sizeof(iNode) - block*BLOCKSIZE);
	return byte;
}

int allocateFreeBlock(){
	//This method should return the address of a free block after removing it from the free-block bitmap
	//Searching in bitmap for an unallocated block

	int blocksReq = (NUMBEROFBLOCKS+(BLOCKSIZE-1))/(BLOCKSIZE); //Adding blockSize to roundup.
	//We are going to write 1s in every bit
	unsigned char buffer[512];
	//Compute start block:
	int currentBlock = NUMBEROFBLOCKS-blocksReq;

	for(currentBlock;currentBlock<NUMBEROFBLOCKS;currentBlock++){
		read_blocks(currentBlock,1,&buffer);
		int i;
		for(i=0;i<BLOCKSIZE;i++){
			if(buffer[i]==FREE){
				changeBlockStatus(((currentBlock - (NUMBEROFBLOCKS-blocksReq))*512 + i),IN_USE);
				return (currentBlock - (NUMBEROFBLOCKS-blocksReq))*512 + i;
			}
		}
	}

	return -1;
}

int changeBlockStatus(int blockNumber, int status){
	//This method should free up a block
	 int correspondingChar = blockNumber%512;
	 int correspondingBlock = (blockNumber+511)/512;
	 int blockToLookUp = correspondingBlock + (NUMBEROFBLOCKS-((NUMBEROFBLOCKS+(BLOCKSIZE-1))/(BLOCKSIZE))) -1;
	 //printf("Blocknumber: %d correspondingChar: %d correspondingBlock: %d blockToLookUp: %d\n", blockNumber, correspondingChar,correspondingBlock,blockToLookUp);
	 unsigned char buffer[512];
	 read_blocks(blockToLookUp,1,&buffer);
	 buffer[correspondingChar] = status;
	 write_blocks(blockToLookUp,1,buffer);
	 return 0;
}

int createINode( long mode, long linkCnt, long uid, long gid, long size){
	iNode newFileINode;
	newFileINode.mode = mode;
	newFileINode.linkCnt = linkCnt;
	newFileINode.uid = uid;
	newFileINode.gid = gid;
	newFileINode.size = size;
	//Calculate the number of blocks required from the size:
	int numBlocksToAllocate = size/BLOCKSIZE + 1;

	if(numBlocksToAllocate>(12+BLOCKSIZE/4)){
		//If the file is too large to be allocated
		printf("%s\n", "File too large to be created");
		return -1;
	}

	if(numBlocksToAllocate>=12){
		//In this case we need to use indirect pointers
		indirectINode indirect;
		newFileINode.indirectPointer = allocateFreeBlock();
		while(numBlocksToAllocate>=12){
			//allocate using indirect pointers
			indirect.pointers[numBlocksToAllocate] = allocateFreeBlock();
			numBlocksToAllocate--;
		}
		//Write the indirect INode to disk;
		unsigned char buffer[512];
		memcpy(buffer,&indirect,512);
		write_blocks(newFileINode.indirectPointer,1,buffer);
	}
	else {
		newFileINode.indirectPointer = FREE;
	}
	int i;
	for(i=0;i<12;i++){
		if(numBlocksToAllocate>0){
			newFileINode.directPointers[i] = allocateFreeBlock();
			numBlocksToAllocate--;
		}
		else{
			newFileINode.directPointers[i] = FREE;
		}
	}

	int numI = findEmptyINode();
	unsigned char blockBuffer[512];
	//Reading the corresponding block from disk
	read_blocks(findINodeBlockAddress(numI),1,&blockBuffer);
	//add the buffer containing the iNode to the block

	memcpy((void*)(blockBuffer+findINodeByteAddress(numI)),&newFileINode,sizeof(newFileINode));

	write_blocks(findINodeBlockAddress(numI),1,&blockBuffer);

	return numI;
}


int mksfs(int fresh) {

	int diskinit;

	//Create a new filesystem if the fresh variable is set
	if(fresh==1){
		diskinit = init_fresh_disk(FILESYSTEMNAME, BLOCKSIZE, NUMBEROFBLOCKS);
		if(diskinit != 0){
			//perror("Error creating disk");
			exit(-1);
		}
		else {
			//To keep the root directory in memory
			initDirectory();


			//Initialize SuperBlock
			mySuperBlock.magic = MAGIC;
			mySuperBlock.blockSize = BLOCKSIZE;
			mySuperBlock.fileSystemSize = NUMBEROFBLOCKS;
			mySuperBlock.iNodeTableLength = NUMBEROFINODES;
			mySuperBlock.rootINode = 0;

			//Use a block sized buffer
			unsigned char buffer[512];
			memset(buffer,255,512);
			memcpy(buffer,&mySuperBlock,512);
			write_blocks(0,1,buffer);

			initializeFreeBitMap();
			initializeFDTable();

			//Now for the Inodes: initialize the first one: (for root dir)
			int numRootINode = createINode(RWE, 1, ROOT_UID, ROOT_GID, 1);

			readDirectory();

			return 0;
		}
	}

	//if the free variable is not set, load
	else if (fresh==0) {
		diskinit = init_disk(FILESYSTEMNAME, BLOCKSIZE, NUMBEROFBLOCKS);
		if(diskinit != 0){
			//perror("Error reading disc");
			exit(-1);
		}
		else {
			return 0;
		}
	}

	return -1;
}


int sfs_fopen(char *name){
	int iNode;
	printf("Opening file");
	iNode = findDirectoryEntry(name);

	if(iNode<0) {
		iNode = createINode(RWE,0,USER_UID,USER_GID,0);
		addToDirectory(name,iNode);
		int fd = addToFDTable(iNode);
		return fd;
	}
	else{
		int fd = addToFDTable(iNode);
		return fd;
	}
}
int sfs_fclose(int fileID){
	return removeFromFDTable(fileID);
}
int sfs_fwrite(int fileID, const char *buf, int length){
	int numINode = fileDescTable[fileID].iNode;
	int writePointer = fileDescTable[fileID].RWPointer;

	unsigned char blockBuffer[512];
	read_blocks(findINodeBlockAddress(numINode),1,&blockBuffer);
	iNode fileINode;
	//memcpy((void*)(blockBuffer+findINodeByteAddress(numI)),buffer,sizeof(newFileINode));
	memcpy(&fileINode,(void*)(blockBuffer+findINodeByteAddress(numINode)),sizeof(iNode));

	if((writePointer+length)>fileINode.size){
		//grow file
		int sizeToAllocate = (writePointer+length)-fileINode.size;
		int blocksToAllocate = ceil(((double)(writePointer+length))/(double)BLOCKSIZE);
		int blocksAlreadyAllocated = ceil (((double)fileINode.size)/(double)BLOCKSIZE);
		if((blocksAlreadyAllocated+blocksAlreadyAllocated)>(12+BLOCKSIZE/4)){
			printf("File too large to be written");
			return -1;
		}
		int i;
		indirectINode indirect;
		for(i=blocksAlreadyAllocated;i<blocksToAllocate;i++){
			if(blocksAlreadyAllocated<12){
				fileINode.directPointers[blocksAlreadyAllocated]=allocateFreeBlock();
			}
			else if(blocksAlreadyAllocated==12 && fileINode.indirectPointer==0){
				fileINode.indirectPointer = allocateFreeBlock();
				indirect.pointers[blocksAlreadyAllocated]=allocateFreeBlock();
			}
			else{
				indirect.pointers[blocksAlreadyAllocated]=allocateFreeBlock();
			}
		}
		//write indirect inode
		if(blocksAlreadyAllocated>11){
			memcpy(blockBuffer,&indirect,sizeof(indirect));
			write_blocks(fileINode.indirectPointer,1,&blockBuffer);
		}
		//Write direct inode
		fileINode.size+=sizeToAllocate;;
		memcpy(blockBuffer,&fileINode,sizeof(fileINode));
		write_blocks(findINodeBlockAddress(numINode),1,&blockBuffer);

	}

	//Write

	return 0;
}
int sfs_fread(int fileID, char *buf, int length){
	int numINode = fileDescTable[fileID].iNode;
	if(numINode==0){
		printf("File not open");
		return -1;
	}
	int readPointer = fileDescTable[fileID].RWPointer;
	int bytesRead;

	unsigned char blockBuffer[512];
	read_blocks(findINodeBlockAddress(numINode),1,&blockBuffer);
	iNode fileINode;
	memcpy(&fileINode,(void*)(blockBuffer+findINodeByteAddress(numINode)),sizeof(iNode));

	int sizeToRead=0;
	int totalRead=0;

	if((readPointer+length)>fileINode.size){
		printf("%s\n", "File smaller than requested");
		return -1;
	}
	else{
		//Find the block to start reading from:
		int currBlock = readPointer/512;
		int currByte = readPointer%512;
		while(currBlock<12 && bytesRead<length){
			read_blocks(fileINode.directPointers[currBlock],1,&blockBuffer);
			if(currByte == 0 && length>512){
				sizeToRead=512;
			}
			else if(currByte!=0 && length>512){
				sizeToRead=512-currByte;
			}
			else if(currByte+length>512){
				sizeToRead=512;
			}
			else{
				sizeToRead=length-currByte;
			}
			memcpy((buf+bytesRead),blockBuffer+currByte,sizeToRead);
			readPointer += sizeToRead;
			length -= sizeToRead;
			totalRead+=sizeToRead;
			int currBlock = readPointer/512;
			int currByte = readPointer%512;
		}
		//Add indirect pointers
	}
	fileDescTable[fileID].RWPointer = readPointer;
	return totalRead;
}
int sfs_fseek(int fileID, int offset){
	if(fileDescTable[fileID].iNode==0){
		printf("File not open");
		return -1;
	}
	fileDescTable[fileID].RWPointer=offset;
	return 0;
}
int sfs_remove(char *file){
	int index = findDirectoryEntry(file);
	if(index==-1){
		printf("%s\n", "File does not exist");
		return -1;
	}
	int numINode = directory[index].iNode;
	clearINode(numINode);
	clearDirectoryAtIndex(index);
	return 0;
}
int sfs_get_next_filename(char* filename){
	if(directory[positionInDir].iNode == 0){
		positionInDir=0;
	}
	strcpy(filename,directory[positionInDir].name);
	positionInDir++;
	return positionInDir-1;
}
int sfs_GetFileSize(const char* path){
	int index = findDirectoryEntry(path);
	if(index==-1){
		printf("%s\n", "File does not exist");
		return -1;
	}
	int numINode = directory[index].iNode;

	unsigned char blockBuffer[512];
	read_blocks(findINodeBlockAddress(numINode),1,&blockBuffer);
	iNode fileINode;
	memcpy(&fileINode,(void*)(blockBuffer+findINodeByteAddress(numINode)),sizeof(iNode));

	return fileINode.size;
}
