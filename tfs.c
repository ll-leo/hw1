/*
 *  Copyright (C) 2019 CS416 Spring 2019
 *	
 *	Tiny File System
 *
 *	File:	tfs.c
 *  Author: Yujie REN
 *	Date:	April 2019
 *
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>

#include "block.h"
#include "tfs.h"

char diskfile_path[PATH_MAX];
bitmap_t i_bitmap = NULL;
bitmap_t d_bitmap = NULL;

int inodes_per_block = BLOCK_SIZE/sizeof(struct inode);
double number_entries = BLOCK_SIZE/sizeof(struct dirent);

struct superblock *superb;
// Declare your in-memory data structures here

/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

	// Step 1: Read inode bitmap from disk
	if(bio_read(superb->i_bitmap_blk,i_bitmap)<=0)
	printf("error bio read fail");
	
	// Step 2: Traverse inode bitmap to find an available slot
	for(int i = 0; i< MAX_INUM; i++){
		if(get_bitmap(i_bitmap, i) == 0){
			
	// Step 3: Update inode bitmap and write to disk 
			set_bitmap(i_bitmap, i);
			bio_write(superb->i_bitmap_blk,i_bitmap);
			return i;
		}
	}
	printf("error no more space");

	return -1;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

	// Step 1: Read data block bitmap from disk
	if(bio_read(superb->d_bitmap_blk, d_bitmap)<=0)
	printf("error bio read fail");
	// Step 2: Traverse data block bitmap to find an available slot
	for(int i=0; i<MAX_DNUM; i++){
		if(get_bitmap(d_bitmap,i) == 0){
	// Step 3: Update data block bitmap and write to disk
			set_bitmap(d_bitmap,i);
			bio_write(superb->d_bitmap_blk,d_bitmap);
			return i;
		}
	
}
	printf("error no more space");
	return -1;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {

  // Step 1: Get the inode's on-disk block number
	int block_number = superb->i_start_blk + ino/inodes_per_block;
	void* cur_block = malloc(BLOCK_SIZE);
	bio_read(block_number,cur_block);
	
	
  // Step 2: Get offset of the inode in the inode on-disk block
	int offset = (ino%inodes_per_block)*sizeof(inode);
  // Step 3: Read the block from disk and then copy into inode structure
  	struct inode* temp = (struct inode*)malloc(sizeof(struct inode));
  	memcpy(temp, cur_block + offset, sizeof(struct inode));
  	free(cur_block);
  	
  	memcpy(inode, temp , sizeof(struct inode));
  	

	return 0;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the block number where this inode resides on disk
	int block_number = superb->i_start_blk + ino/inodes_per_block;
	void* cur_block = malloc(BLOCK_SIZE);
	if(bio_read(block_number,cur_block)<0);
	printf("fail to Step 1: Get the block number where this inode resides on disk");
	// Step 2: Get the offset in the block where this inode resides on disk
	int offset = (ino%inodes_per_block)*sizeof(inode);
	// Step 3: Write inode to disk 
	struct inode* temp = (struct inode*) (cur_block+offset);
	*temp = *inode;
	
	if(bio_write(block_number,cur_block)<0)
	printf("fail to Step 3: Write inode to disk");

	return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

  // Step 1: Call readi() to get the inode using ino (inode number of current directory)
	struct inode* cur_dir_inode = malloc(sizeof(struct inode));
	readi(ino,cur_dir_inode);
	int cur_offset = 0;
  // Step 2: Get data block of current directory from inode
	int *cur_dir_data = cur_dir_inode->direct_ptr;
	struct dirent *temp_block = (struct dirent *)malloc(BLOCK_SIZE);
  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure
  //read data block
  for(int i=0; i<16; i++){
  	if(cur_dir_data[i]!=-1){
  		
  		//offset
  		cur_offset = superb->d_start_blk + cur_dir_data[i];
  		bio_read(cur_offset,temp_block);
  		
  		//check entry
  		for(int j = 0; i<number_entries ; j++){
  			
  			if(temp_block[j].vail!=0 && temp_block[j] != NULL){
  				
  				if(strcmp(temp_block[j]->name, fname) == 0){
  					*dirent = *temp_block[j];
  					
  					free(cur_dir_inode);
  					free(temp_block);
  					free(cur_dir_data);
  					return cur_offset;
  					
  				}
  				
  			}
  		}
  		
  		
  	}
  }
	printf("cannot find match file name");
	return -1;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	int *cur_dir = malloc(16*sizeof(int));
	memcpy(cur_dir,dir_inode.direct_ptr,16*sizeof(int));
	struct dirent *temp_block = malloc(BLOCK_SIZE);
	
	// Step 2: Check if fname (directory name) is already used in other entries
	if(dir_find(dir_inode.ino, fname, name_len,temp_block)!=-1)
	{
		printf("this fname is used by other");
		return -1;
	}
	
	

	struct dirent *new_entry = malloc(sizeof(struct dirent));
	new_entry->valid = 1;
	new_entry->ino = f_ino;
	strncpy(new_entry->name,fname,name_len+1);
	
	//find empty block
	int empty_unallocate_block = -1;
	int empty_allocate_block = -1;
	for(int i=0; i<16; i++){
		if(cur_dir[i]==-1&&empty_unallocate_block == -1){
			empty_unallocate_block = i;
		}
		else if(cur_dir[i]!=-1){
			int offset = superb->d_start_blk + cur_dir[i];
			bio_read(offset,temp_block);
			for(int j=0; j<number_entries; j++){
				if(temp_block[j].valid == 0){
					if(empty_allocate_block == -1)
					{
						empty_allocate_block = i;
					}
				}
			}
			
		}
	}
	
	// Step 3: Add directory entry in dir_inode's data block and write to disk
	if(empty_allocate_block != -1){
		
		bio_read(superb->d_start_blk + cur_dir[empty_allocate_block],temp_block);
		
		temp_block[empty_allocate_block]=*new_entry;
		
		bio_write(super->d_start_blk + cur_dir[empty_allocate_block],temp_block);
		
	}
	// Allocate a new data block for this directory if it does not exist
	else if(empty_unallocate_block != 1){
		
		int ava_block = get_avail_blkno();
		
		cur_dir[empty_unallocate_block] = ava_block;
		
		struct dirent *new_dir = malloc(BLOCK_SIZE);
		
		new_dir[0] = *new_entry;
		
		bio_write(superb->d_start_blk+ava_block, new_dir);
		
		free(new_dir);
		
	}
	
	// Update directory inode
	struct inode *directory_inode = malloc(sizeof(struct inode));
	readi(dir_inode.ino, directory_inode);
	
	directory_inode->size = directory_inode->size + sizeof(struct dirent);
	
	memcpy(directory_inode->direct_ptr, cur_dir, 16*sizeof(int));
	
	writei(dir_inode.ino, directory_inode);
	// Write directory entry
	
	
	
	free(directory_inode);
	free(new_entry);
	free(temp_data);
	free(cur_dir);
	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	struct dirent *remove_target = malloc(sizeof(struct dirent));
	int offset = dir_find(dir_inode.ino, fname, name_len, remove_target);
	// Step 2: Check if fname exist
	if(offset == -1){
		printf("did not find the target to remove");
		return offset;
	}

	// Step 3: If exist, then remove it from dir_inode's data block and write to disk
	
	struct dirent* temp_block = malloc(BLOCK_SIZE);
	
	bio_read(offset, temp_block);
	
	for(int i=0; i<number_entries; i++){
		//may also work for inode?
		if(temp_block[i] == offset-superb->d_start_blk){
			struct inode *temp_inode = malloc(sizeof(struct inode));
			readi(dir_inode.ino, temp_inode);
			
			temp_inode->direct_ptr[i] = -1;
			writei(dir_inode.ino, temp_inode);
			
			
		}
		
		
	}

	return 0;
}

/* 
 * namei operation
 */
 void initial_char(char *str){
 	for(int i=0; i<1024; i++){
 		str[i] = '\0';
 	}
 	
 }
 
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way
	char* new_path = malloc(1024*sizeof(char));
	char* cur_path = malloc(1024*sizeof(char));
	initial_char(new_path);
	initial_char(cur_path);
	int if_new_turn = 0;
	int cur_length = 0;
	int new_length = 0;
	for(int i=1; path[i]!='\0'&&path[i]!=NULL ; i++){
		if(if_new_turn == 0){
			if(cur_path[i]!='/'){
			
			cur_path[i-1] = path[i];
			cur_length++;	
		}
			else{
			cur_path[i-1] = '\0';
			if_new_turn = 1;
		}
		}
		
		else if(if_new_turn == 1){
			new_path[i-cur_length-1] = path[i];
			new_length ++;
		}
		
	}
	new_path[new_length] = '\0';
	
	if(new_length!=0){
		
		struct dirent *dir = malloc(sizeof(struct dirent));
		dir_find(ino, cur_path, cur_length, dir);
		int number_st = get_node_by_path(new_path, dir->ino, inode);
		free(dir);
		free(new_path);
		free(cur_path);
		if(number_st != 0){
			return -1;
		}
	}
	else{
		struct inode *final = malloc(sizeof(struct inode));
		struct dirent *dir = malloc(sizeof(struct dirent));
		dir_find(ino,cur_path, cur_length, dir);
		readi(dir->ino, final);
		memcpy(inode, final, sizeof(struct inode));
		free(dir);
		free(final);
		free(new_path);
		free(cur_path);
		return 0;
		
	}
	
	
	
	
	
	
	return -1;
}

/* 
 * Make file system
 */
int tfs_mkfs() {

	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);
	// write superblock information
	i_bitmap = (bitmap_t)malloc(BLOCK_SIZE);
	d_bitmap = (bitmap_t)malloc(BLOCK_SIZE);
	superb->magic_num = MAGIC_NUM;
	superb->max_dnum = MAX_INUM;
	superb->max_inum = MAX_DNUM;
	superb->i_bitmap_blk = 1;
	superb->d_bitmap_blk = 2;
	superb->i_start_blk = 3;
	superb->d_start_blk = superb->i_start_blk + MAX_INUM/inodes_per_block;
	// initialize inode bitmap
	for(int i=0;i<MAX_INUM; i++){
		
		i_bitmap[i] = 0;
		
	}
	
	// initialize data block bitmap
	for(int i=0;i<MAX_DNUM; i++){
		d_bitmap[i] = 0;
	}
	bio_write(0, superb);
	bio_write(superb->i_bitmap_blk, i_bitmap);
	bio_write(superb->d_bitmap_blk, d_bitmap);
	struct inode* root_dir = malloc(sizeof(struct inode));
	root_dir->ino = get_avail_ino();
	root_dir->valid = 1;
	root_dir->size = 0;
	//root_dir->type = DIR;
	root_dir->link = 2;
	//inital for vstat?
	for(int i=0; i<16; i++){
		root_dir->direct_ptr[i] = -1;
		if(i<8){
			root_dir->indirect_ptr[i] = -1;
		}
	}
	// update bitmap information for root directory

	// update inode for root directory
	writei(0,root_dir);
	return 0;
}


/* 
 * FUSE file operations
 */
static void *tfs_init(struct fuse_conn_info *conn) {

	// Step 1a: If disk file is not found, call mkfs
	int disk_found = dev_open(diskfile_path);
	if(disk_found == -1){
		tfs_mkfs();
		
	}
  // Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk
  	else{
  		superb = (struct superblock*)malloc(BLOCK_SIZE);
  		bio_read(0,superb);
  		if(superb->magic_num != MAGIC_NUM){
  			printf("not right disk");
  			exit(-1);
  		}
  	}

	return NULL;
}

static void tfs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures
	bio_write(0, superb);
	bio_write(superb->i_bitmap_blk,i_bitmap);
	bio_write(superb->d_bitmap_blk,d_bitmap);
	free(superb);
	
	
	// Step 2: Close diskfile
	dev_close(diskfile_path);

}

static int tfs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path
	struct inode* cur_inode = malloc(sizeof(struct inode));
	get_node_by_path(path,0,cur_inode);
	
	// Step 2: fill attribute of file into stbuf from inode
		stbuf->st_atime = cur_inode->vstat.st_atime;
		stbuf->st_ctime = cur_inode->vstat.st_ctime;
		stbuf->st_dev = cur_inode->vstat.st_dev;
		stbuf->st_gid = cur_inode->vstat.st_gid;
		stbuf->st_ino = cur_inode->vstat.st_ino;
		stbuf->st_mode   = S_IFDIR | 0755;
		stbuf->st_nlink  = 2;
		time(&stbuf->st_mtime);
	free(cur_inode);
	return 0;
}

static int tfs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode *cur_inode = (struct inode*)malloc(sizeof(struct inode));
	if(get_node_by_path(path,0,cur_inode) == 0){
		free(cur_inode);
	}
	
	// Step 2: If not find, return -1
	
	else{
		printf("cannot open dir");
		free(cur_inode);
		return -1;
	}

    return 0;
}

static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode *cur_inode = (struct inode*)malloc(sizeof(struct inode));
	
	if(get_node_by_path(path,1,cur_inode) != 0){
		printf("erorr tfs_readdir get node by path");
		return -1;
	}
	
	// Step 2: Read directory entries from its data blocks, and copy them to filler
	struct dirent *dir_entry = (struct dirent *)malloc(BLOCK_SIZE);
	for(int i=0; i<16 ;i++){
		// ? 
		offset = superb->d_start_blk + cur_inode->direct_ptr[i];
		bio_read(offset, dir_entry);
		for(int j=0; j<number_entries; j++){
			
			if(dir_entry[i].valid == 1){
				if(filler(buffer,dir_entry[i].name, NULL,offset) == -1)
				return 0;
			}
		}
		
		
	}
	
	free(dir_entry);
	free(cur_inode);
	return 0;
}

int dirname_basename(const char *path, char *dir_name, char *base_name){
	int number_dash = 0;
	int locate_last_dash = 0;
	for(int i=0; i<strlen(path); i++){
		if(path[i] == '/'){
			number_dash ++;
			locate_last_dash = i;
		}
		
	}
	
	for(int i=0; i<strlen(path); i++){
		
		if(i<locate_last_dash){
			dir_name[i] = path[i];
		}
		else if(i>locate_last_dash){
			base_name[i-locate_last_dash] = path[i]; 
		}
	}

	return 0;	
	
}


static int tfs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char *dir_name = malloc(1024*sizeof(char));
	char *base_name = malloc(1024*sizeof(char));
	initial_char(dir_name);
	initial_char(base_name);
	dirname_basename(path,dir_name,base_name);
	
	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode *parent_dir_inode = malloc(sizeof(struct inode));
	
	if(get_node_by_path(dir_name,0,parent_dir_inode) == -1){
		printf("false tfs_mkdir");
		return -1;
	}
	// Step 3: Call get_avail_ino() to get an available inode number
	struct inode* new_dir = malloc(sizeof(struct inode));
	new_dir->ino = get_avail_ino();
	new_dir->valid = 1;
	new_dir->size = 0;
	//new_dir->type = DIR;
	new_dir->link = 2;
	//inital for vstat?
	for(int i=0; i<16; i++){
		new_dir->direct_ptr[i] = -1;
		if(i<8){
			new_dir->indirect_ptr[i] = -1;
		}
	}
	
	
	
	
	// Step 4: Call dir_add() to add directory entry of target directory to parent directory
	if(dir_add(*parent_dir_inode, new_dir->ino, strlen(base_name)) == -1){
		printf("error mkdir");
		return -1;
	}
	// Step 5: Update inode for target directory
	
	// Step 6: Call writei() to write inode to disk
	writei(new_dir->ino, new_dir);

	free(dir_name);
	free(base_name);
	free(parent_dir_inode);
	free(new_dir);
	return 0;
}

static int tfs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char *dir_name = malloc(1024*sizeof(char));
	char *base_name = malloc(1024*sizeof(char));
	initial_char(dir_name);
	initial_char(base_name);
	dirname_basename(path,dir_name,base_name);
	// Step 2: Call get_node_by_path() to get inode of target directory
	struct inode *parent_dir_inode = malloc(sizeof(struct inode));
	struct inode *target_inode = malloc(sizeof(struct inode));
	
	
	if(get_node_by_path(dir_name,0,parent_dir_inode) == -1){
		printf("false tfs_rmdir");
		return -1;
	}
	if(get_node_by_path(path,0,target_inode) == -1){
		printf("false tfs_rmdir");
		return -1;
	}
	// Step 3: Clear data block bitmap of target directory
	
	bio_read(2,d_bitmap);
	dir_remove(parent_dir_inode, base_name, strlen(base_name));
	unset_bitmap(d_bitmap, target_inode->ino);
	bio_write(2,d_bitmap);
	// Step 4: Clear inode bitmap and its data block
	bio_read(1,i_bitmap);
	unset_bitmap(i_bitmap, target_inode->ino);
	bio_write(1,i_bitmap);
	
	free(dir_name);
	free(base_name);
	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory

	return 0;
}

static int tfs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	char *parent_dir, *target_file;

	struct inode *parent_inode=malloc(sizeof(struct inode));
	// int ino_parent_dir;
	//	struct inode *target_file_inode=malloc(sizeof(struct inode));

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name
	char *dir_name = malloc(1024*sizeof(char));
	char *base_name = malloc(1024*sizeof(char));
	initial_char(dir_name);
	initial_char(base_name);
	dirname_basename(path,dir_name,base_name);
	// Step 2: Call get_node_by_path() to get inode of parent directory
	if(get_node_by_path(dir_name, 0, parent_inode) == -1){
		printf("dire not find");
		return -1;
	} 
	
	// Step 3: Call get_avail_ino() to get an available inode number
	struct inode* new_inode = malloc(sizeof(struct inode));
	new_inode->ino = get_avail_ino();
	new_inode->valid = 1;
	new_inode->size = 0;
	new_inode->link = 1;
	//new_inode->type = TFS_FILE;
	for(int i=0; i<16; i++){
		new_inode->direct_ptr[i] = -1;
		if(i<8){
			new_inode->indirect_ptr[i] = -1;
			
		}
	}
	
	// Step 4: Call dir_add() to add directory entry of target file to parent directory
	if(dir_add(*parent_inode, new_inode->ino, base_name, strlen(base_name)) == -1){
		printf("tfs_create fail");
		return -1;
	}
	
	// Step 5: Update inode for target file
	
	// Step 6: Call writei() to write inode to disk
	writei(new_inode->ino, new_inode);
	
	free(new_inode);
	free(parent_inode);
	free(dir_name);
	free(base_name);
	
	return 0;
}

static int tfs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode* cur_inode = malloc(BLOCK_SIZE);
	if(get_node_by_path(path,0,cur_inode) == -1){
		
		free(cur_inode);
		return -1;
	}
	// Step 2: If not find, return -1
	free(cur_inode);
	return 0;
}

static int tfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	
	// Step 1: You could call get_node_by_path() to get inode from path
	struct inode* cur_inode = malloc(BLOCK_SIZE);
	if(get_node_by_path(path,0,cur_inode) == -1){
		printf("tfs_read fail");
		return -1;
	}
	// Step 2: Based on size and offset, read its data blocks from disk
	
	int number_block = offset/BLOCK_SIZE + 1;
	int number_byte_offset = offset%BLOCK_SIZE;
	int* data_block = malloc(BLOCK_SIZE);
	
	
	printf("--- running tfs_read\n");
	
	
	
	// Step 3: copy the correct amount of data from offset to buffer
	
	return 0;
	// Note: this function should return the amount of bytes you copied to buffer

}

static int tfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
	printf("--- running tfs_write");
	return size;
}

static int tfs_unlink(const char *path) {
	
	printf("---running tfs_unlink");
	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of target file

	// Step 3: Clear data block bitmap of target file

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory

	return 0;
}

static int tfs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int tfs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations tfs_ope = {
	.init		= tfs_init,
	.destroy	= tfs_destroy,

	.getattr	= tfs_getattr,
	.readdir	= tfs_readdir,
	.opendir	= tfs_opendir,
	.releasedir	= tfs_releasedir,
	.mkdir		= tfs_mkdir,
	.rmdir		= tfs_rmdir,

	.create		= tfs_create,
	.open		= tfs_open,
	.read 		= tfs_read,
	.write		= tfs_write,
	.unlink		= tfs_unlink,

	.truncate   = tfs_truncate,
	.flush      = tfs_flush,
	.utimens    = tfs_utimens,
	.release	= tfs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &tfs_ope, NULL);

	return fuse_stat;
}

