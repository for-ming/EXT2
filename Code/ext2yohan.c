#include "disksim.h"
#include "ext2.h"
#include<stdio.h>

int ext2_dump(DISK_OPERATIONS* disk, int blockGroupNum, int type, int target){
	int i, j, blockNum, sectorCnt;
	BYTE* blockSector[SECTOR_SIZE * SECTORS_PER_BLOCK];
	UINT32 group_count = disk->numberOfSectors * disk->bytesPerSector / GROUP_SIZE_IN_BYTES;
	UINT32 descBlockCount = group_count * sizeof(EXT2_GROUP_DESC) / BLOCK_SIZE_IN_BYTES;
	UINT32 inodeBlockCount = (INODES_PER_GROUP * INODE_SIZE_IN_BYTES ) / BLOCK_SIZE_IN_BYTES;

	switch(type){
		case 1:
			blockNum = 0;
			sectorCnt = 2;
			break;
		case 2:
			blockNum = 1;
			sectorCnt = descBlockCount * SECTORS_PER_BLOCK;
			break;
		case 3:
			blockNum = BLOCK_PER_GROUP * blockGroupNum + 1 + descBlockCount;
			sectorCnt = SECTORS_PER_BLOCK;
			break;
		case 4:
			blockNum = BLOCK_PER_GROUP * blockGroupNum + 2 + descBlockCount;
			sectorCnt = SECTORS_PER_BLOCK;
			break;
		case 5:
			blockNum = BLOCK_PER_GROUP * blockGroupNum + 3 + descBlockCount;
			sectorCnt = inodeBlockCount * SECTORS_PER_BLOCK;
			break;
		case 6:
			blockNum = BLOCK_PER_GROUP * blockGroupNum + 3 + descBlockCount + inodeBlockCount;
			sectorCnt = SECTORS_PER_BLOCK;
			break;
	}

	for(i=0;i<SECTORS_PER_BLOCK;i++)
		disk->read_sector(disk, blockNum * SECTORS_PER_BLOCK + i + target,blockSector+SECTOR_SIZE*i);

	printf("start adress : %p , end address : %p\n\n\n",blockSector,blockSector+SECTOR_SIZE*sectorCnt);
	
	for(i=0;i<SECTOR_SIZE * sectorCnt;i++){
		if(i % SECTOR_SIZE == 0 && i !=0)
			for(j=i;j<i+SECTORS_PER_BLOCK;j++)
				disk->read_sector(disk, blockNum * SECTORS_PER_BLOCK + j + target,blockSector+SECTOR_SIZE*j);	

		if(i%16 == 0)
			printf("%p\t%x ",&blockSector[i],blockSector[i]);
		else if(i % 16==15)
			printf(" %x \n",blockSector[i]);
		else
			printf(" %x ",blockSector[i]);
	}
	printf("\n\n");

	return 0;
}
