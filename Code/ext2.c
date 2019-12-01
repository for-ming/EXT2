#include<stdio.h>
#include "disksim.h"
#include "ext2.h"

unsigned char toupper( unsigned char ch );
int isalpha( unsigned char ch );
int isdigit( unsigned char ch );

void upper_string( char* str, int length )
{
	while( *str && length-- > 0 )
	{
		*str = toupper( *str );
		str++;
	}
}

int set_bit(int number, BYTE* sector){
	BYTE value;
	int byte=number/8;
	int offset=number%8;

	switch(offset){
		case 0:
			value = 0x80;
			break;
		case 1:
			value = 0x40;
			break;
		case 2:
			value = 0x20;
			break;
		case 3:
			value = 0x10;
			break;
		case 4:
			value = 0x8;
			break;
		case 5:
			value = 0x4;
			break;
		case 6:
			value = 0x2;
			break;
		case 7:
			value = 0x1;
			break;
	}
	sector[byte] |= value;
}

int init_bitmap(DISK_OPERATIONS* disk, EXT2_SUPER_BLOCK *sb, EXT2_GROUP_DESC *desc, int group_number){
	BYTE sector[MAX_SECTOR_SIZE];
	int i;
	int reserved;
	int datablock, inodeblock;
	
	//block bitmap
	ZeroMemory(sector, MAX_SECTOR_SIZE);

	for(i=0;i<SECTORS_PER_BLOCK;i++){
		disk->write_sector(disk, i + (desc->bg_block_bitmap * SECTORS_PER_BLOCK)/* 8 */ + group_number * sb->s_blocks_per_group * SECTORS_PER_BLOCK, sector);
	}

	datablock = sb->s_first_data_block + desc->bg_inode_table + (sb->s_inodes_per_group * sizeof(EXT2_INODE) / BLOCK_SIZE_IN_BYTES);
	for(i=0;i<datablock;i++)
		set_bit(i, sector);
	
//	printf("+++++++++++%d+++++++++++\n", desc->bg_block_bitmap * SECTORS_PER_BLOCK + group_number * sb->s_blocks_per_group * SECTORS_PER_BLOCK);
//	for(i=0;i<20;i++)
//		printf("%x ", sector[i]);
//	printf("\n");
	disk->write_sector(disk, (desc->bg_block_bitmap * SECTORS_PER_BLOCK) + (group_number * sb->s_blocks_per_group * SECTORS_PER_BLOCK), sector);

	//inode bitmap
	ZeroMemory(sector, MAX_SECTOR_SIZE);	

	for(i=0;i<SECTORS_PER_BLOCK;i++){
		disk->write_sector(disk, i + desc->bg_inode_bitmap * SECTORS_PER_BLOCK + group_number * sb->s_blocks_per_group * SECTORS_PER_BLOCK, sector);
	}

	if(group_number == 0){
		for(i=0;i<sb->s_first_ino;i++)
			set_bit(i, sector);
		
			disk->write_sector(disk, desc->bg_inode_bitmap * SECTORS_PER_BLOCK + group_number * sb->s_blocks_per_group * SECTORS_PER_BLOCK, sector);
	}
}

int fill_entry(EXT2_DIR_ENTRY* entry, char* name, int inodeNum){
	if(inodeNum < 0)
		return -1;

	if(strcmp(name, "no_more") == 0){
		entry->name[0]=DIR_ENTRY_NO_MORE;
		return 0;
	}

	entry->inode=inodeNum;
	entry->name_len = strlen(name);
	entry->rec_len= sizeof(UINT32) + sizeof(UINT16) + sizeof(UINT16) + sizeof(char) * entry->name_len;
	memcpy(entry->name, name, entry->name_len);
}

int create_root(DISK_OPERATIONS* disk){
	BYTE dirSector[MAX_SECTOR_SIZE];
	BYTE inodeSector[MAX_SECTOR_SIZE];
	BYTE descSector[MAX_SECTOR_SIZE];
	BYTE sbSector[MAX_SECTOR_SIZE];
	BYTE sector[MAX_SECTOR_SIZE];

	EXT2_SUPER_BLOCK *sb;
	EXT2_GROUP_DESC *desc;
	EXT2_DIR_ENTRY *entry;
	EXT2_INODE *inode;

	int rootInode=2;
	int i=0;

	EXT2_DIR_ENTRY *dotEntry;
	EXT2_DIR_ENTRY *dotdotEntry;

	SECTOR rootSector=0;

	disk->read_sector(disk, 0, sbSector);
	sb = (EXT2_SUPER_BLOCK*) sbSector;
	disk->read_sector(disk, ((sb->s_first_data_block + 1) * SECTORS_PER_BLOCK), descSector);
	desc = (EXT2_GROUP_DESC*) descSector;

	ZeroMemory(inodeSector, MAX_SECTOR_SIZE);
	inode = (EXT2_INODE*) inodeSector;
	inode+=rootInode;

	inode->i_mode=ATTR_VOLUME_ID;
	inode->i_blocks=1;
	inode->i_block[0] = desc->bg_inode_table + (sb->s_inodes_per_group * sizeof(EXT2_INODE) / BLOCK_SIZE_IN_BYTES);

	rootSector = desc->bg_inode_table * SECTORS_PER_BLOCK;
	disk->write_sector(disk, rootSector, inodeSector);
	
	ZeroMemory(dirSector, MAX_SECTOR_SIZE);
	entry = (EXT2_DIR_ENTRY*) dirSector;

	fill_entry(entry, "/", 2);//root_dir_entry
	entry++;
	fill_entry(entry, ".", 2);//.
	entry++;
	fill_entry(entry, "..", 2);//..
	entry++;
	fill_entry(entry, "no_more", 2);//NO_MORE

	UINT32	datablock = sb->s_first_data_block + desc->bg_inode_table + (sb->s_inodes_per_group * sizeof(EXT2_INODE) / BLOCK_SIZE_IN_BYTES);
	disk->write_sector(disk, datablock * SECTORS_PER_BLOCK, dirSector);
	
	//modify super block
	sb->s_free_blocks_count--;

	disk->write_sector(disk, sb->s_first_data_block * SECTORS_PER_BLOCK, sb);
	
	//modify group desc
	desc->bg_free_blocks_count--;
	desc->bg_used_dirs_count++;

	//write group desc sector
	disk->write_sector(disk, (sb->s_first_data_block + 1) * SECTORS_PER_BLOCK, descSector);


	//Datablock bitmap
	UINT32 sectorNum = datablock / (8 * MAX_SECTOR_SIZE);
	disk->read_sector(disk, (desc->bg_block_bitmap * SECTORS_PER_BLOCK) + sectorNum, sector);

//	printf("----------%d----------\n", (desc->bg_block_bitmap * SECTORS_PER_BLOCK) + sectorNum);
//	printf("%d\n%d\n%d\n", desc->bg_block_bitmap, SECTORS_PER_BLOCK, sectorNum);

//	for(i=0;i<20;i++)
//		printf("%x ", sector[i]);
//	printf("\n");
	set_bit(datablock, sector);
//	for(i=0;i<20;i++)
//		printf("%x ", sector[i]);
//	printf("\n");
	disk->write_sector(disk, desc->bg_block_bitmap * SECTORS_PER_BLOCK + sectorNum, sector);

	return 0;
}

int fill_desc(DISK_OPERATIONS *disk, EXT2_SUPER_BLOCK *sb, EXT2_GROUP_DESC *entry){
	UINT32 group_count = disk->numberOfSectors * disk->bytesPerSector / GROUP_SIZE_IN_BYTES;
	UINT32 descBlockCount = group_count * sizeof(EXT2_GROUP_DESC) / BLOCK_SIZE_IN_BYTES;
	if( group_count * sizeof(EXT2_GROUP_DESC) % BLOCK_SIZE_IN_BYTES != 0 )
		descBlockCount++;

	ZeroMemory(entry, sizeof(EXT2_GROUP_DESC));
	entry->bg_block_bitmap = sb->s_first_data_block + 1 + descBlockCount;
	entry->bg_inode_bitmap = entry->bg_block_bitmap + 1;
	entry->bg_inode_table = entry->bg_inode_bitmap + 1;
	entry->bg_free_blocks_count=sb->s_free_blocks_count / group_count;
	entry->bg_free_inodes_count=sb->s_free_inodes_count / group_count;
	entry->bg_used_dirs_count = 0;
/*
	printf("group Count = %d\n", group_count);
	printf("descBlockCount = %d\n", descBlockCount);
	printf("block_bitmap block = %d\n", entry->bg_block_bitmap);
	printf("inode_bitmap block = %d\n", entry->bg_inode_bitmap);
	printf("inode_table block = %d\n", entry->bg_inode_table);
	printf("bg_free_blocks_count = %d\n", entry->bg_free_blocks_count);
	printf("bg_free_inode_count = %d\n", entry->bg_free_inodes_count);
	printf("bg_used_dirs_count = %d\n", entry->bg_used_dirs_count);
*/
	return EXT2_SUCCESS;
}

int fill_sb(EXT2_SUPER_BLOCK *sb, SECTOR numberOfSectors, UINT32 bytesPerSector){
	QWORD diskSize=numberOfSectors * bytesPerSector;

	ZeroMemory(sb, sizeof(EXT2_SUPER_BLOCK));

	sb->s_blocks_per_group = GROUP_SIZE_IN_BYTES / BLOCK_SIZE_IN_BYTES;
	sb->s_inodes_per_group = INODES_PER_GROUP;

	sb->s_inodes_count = sb->s_inodes_per_group * (diskSize / GROUP_SIZE_IN_BYTES);
	sb->s_blocks_count = sb->s_blocks_per_group * (diskSize / GROUP_SIZE_IN_BYTES);
//	sb->s_r_block_count = 0;
	sb->s_first_ino = 11;
	sb->s_free_blocks_count = sb->s_blocks_count -(diskSize / GROUP_SIZE_IN_BYTES);
	sb->s_free_inodes_count = sb->s_inodes_count - (sb->s_first_ino-1) * (diskSize / GROUP_SIZE_IN_BYTES);
	sb->s_first_data_block = 0;
	sb->s_log_block_size = 4;

	sb->s_magic = 0xEF53;
	sb->s_state = EXT2_VALID_FS;
	sb->s_errors = EXT2_ERRORS_CONTINUE;
	sb->s_minor_rev_level = 0;

	sb->s_rev_level = EXT2_GOOD_OLD_REV;
	
	sb->s_inode_size = INODE_SIZE_IN_BYTES;
	sb->s_block_group_nr = 0;


	printf("inodes count = %x\n", sb->s_inodes_count);
	printf("blocks count = %x\n", sb->s_blocks_count);
	
	printf("free blocks count = %x\n", sb->s_free_blocks_count);
	printf("free inodes count = %x\n", sb->s_free_inodes_count);
	printf("first data block = %x\n", sb->s_first_data_block);
	printf("blocks per group = %x\n", sb->s_blocks_per_group);

	return EXT2_SUCCESS;
}

int ext2_format( DISK_OPERATIONS* disk){
	EXT2_SUPER_BLOCK sb;
	BYTE sector[MAX_SECTOR_SIZE];
	EXT2_GROUP_DESC *desc;
	UINT32 i, j;
	UINT32 group_count = disk->numberOfSectors * disk->bytesPerSector / GROUP_SIZE_IN_BYTES;
	UINT32 descSector=SECTORS_PER_BLOCK;
	
	if(fill_sb(&sb, disk->numberOfSectors, disk->bytesPerSector) !=EXT2_SUCCESS)
		return EXT2_ERROR;

	disk->write_sector(disk, 0, &sb);
	ZeroMemory(sector, MAX_SECTOR_SIZE);
	disk->write_sector(disk, 1, sector);

//	printf("fill_sb finish\n");

	for(i=0;i<group_count;i++){
		if(i%(disk->bytesPerSector / sizeof(EXT2_GROUP_DESC)) == 0){
//			printf("desc sector zero !!!\n");
			ZeroMemory(sector, MAX_SECTOR_SIZE);
			desc=(EXT2_GROUP_DESC *)sector;
		}

		if(fill_desc(disk, &sb, desc) != EXT2_SUCCESS)
			return EXT2_ERROR;
	
		if(i/(disk->bytesPerSector / sizeof(EXT2_GROUP_DESC)) != 0 || i==group_count-1){
			for(j=0;j<group_count;j++){
				disk->write_sector(disk, j * (sb.s_blocks_per_group) * SECTORS_PER_BLOCK + descSector + (i/(disk->bytesPerSector / sizeof(EXT2_GROUP_DESC))), sector);
			}
		}
		init_bitmap(disk, &sb, desc, i);			
		desc++;
	}

	create_root(disk);	
	
	return EXT2_SUCCESS;
}

/*
int main(){
	DISK_OPERATIONS disk;
	disksim_init(524288, 512, &disk);
	disk.bytesPerSector=512;
	disk.numberOfSectors=512*1024;
	ext2_format(&disk);
}
*/

int ext2_dump(DISK_OPERATIONS* disk, int blockGroupNum, int type, int target){
	int i, j, blockNum, sectorCnt;
	BYTE blockSector[MAX_SECTOR_SIZE];
	UINT32 group_count = disk->numberOfSectors * disk->bytesPerSector / GROUP_SIZE_IN_BYTES;
	UINT32 descBlockCount = group_count * sizeof(EXT2_GROUP_DESC) / BLOCK_SIZE_IN_BYTES;
	if( group_count * sizeof(EXT2_GROUP_DESC) % BLOCK_SIZE_IN_BYTES != 0 )
		descBlockCount++;
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
			sectorCnt = SECTORS_PER_BLOCK;
			break;
		case 6:
			blockNum =  target;
			sectorCnt = SECTORS_PER_BLOCK;
			break;
	}
	
	for(i=0;i<sectorCnt;i++){
		disk->read_sector(disk, blockNum * SECTORS_PER_BLOCK + i ,blockSector);	
		if(i==0)
			printf("start adress : %p , end address : %p\n\n",blockSector,blockSector+MAX_SECTOR_SIZE*sectorCnt);
		for(j=0;j<MAX_SECTOR_SIZE;j++){

			if(j%16 == 0)
				printf("\n%p\t ",&blockSector[j]);
			
			if(blockSector[j]<=0xf)
				printf(" 0%x ",blockSector[j]);
			else
				printf(" %x ",blockSector[j]);
		}
	}
	printf("\n\n");

	return 0;
}

int fill_inode(EXT2_INODE* inode, UINT32 inodeNum){
    return 0;}

int ext2_create( EXT2_NODE* EXT2Parent, const char* name, EXT2_NODE* Entry ){
    return 0;}

int ext2_remove( EXT2_NODE* file ){
    return 0;}
 
int ext2_read( EXT2_NODE* file, unsigned long inodeNum, unsigned long length, char* buffer){
    return 0;}

int ext2_write( EXT2_NODE* file, unsigned long inodeNum, unsigned long length, const char* buffer){
    return 0;}
;

int ext2_read_superblock(EXT2_FILESYSTEM* fs, EXT2_NODE* root){
	BYTE	sector[MAX_SECTOR_SIZE];

	if ( fs == NULL || fs->disk == NULL ){
		WARNING ( " DISK OPERATIONS : %p \n FAT_FILESYSTEM : %p \n", fs, fs->disk );
		return EXT2_ERROR;
	}

	if ( fs->disk->read_sector ( fs->disk, 0, &fs->spb ) )
		return EXT2_ERROR;
	
	ZeroMemory ( root, sizeof(EXT2_NODE) );	
	memcpy( &root->entry, sector, sizeof(EXT2_DIR_ENTRY) );
	root->fs = fs;
	
	memset( root->entry.name, 0x20, 11);
	
	root->inodeNum = 2; // root inode
	root->groupNum = 0; // 

	printf("root -> entry.name : %s \n", root->entry.name);
	printf("root -> inode : %d \n",root->inodeNum);

	return EXT2_SUCCESS;
} //minji2.c

int ext2_df( EXT2_FILESYSTEM* fs, UINT32* totalSectors, UINT32* usedSectors ){
	if ( (fs->spb.s_blocks_count) != 0 );
	else
		*totalSectors = fs->spb.s_free_blocks_count * 8;
	
	*usedSectors = (fs->spb.s_blocks_count - fs->spb.s_free_blocks_count) * 8;
	
	return EXT2_SUCCESS;
}//disk free // minji3.c

int ext2_read_dir( EXT2_NODE* dir, EXT2_NODE_ADD adder, void* list){
return 0;}
 
int ext2_lookup( EXT2_NODE* Parent , const char* entryName, EXT2_NODE* retEntry ){
    return 0;}

int ext2_mkdir( EXT2_NODE* Parent, const char* entryName, EXT2_NODE* retEntry ){
    return 0;}

int ext2_rmdir( EXT2_NODE* dir ){
	return 0;
}
