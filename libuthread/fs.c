#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF

size_t get_offset(int fd);
int get_file(int fd);


struct superblock{
    char signature[8];
    uint16_t total_blk;
    uint16_t root_idx;
    uint16_t data_start_idx;
    uint16_t data_blk_amount;
    uint8_t num_FAT_blk;
    uint8_t padding[4079];
};

/** The first entry of the FAT (entry #0) is
 always invalid and contains the special FAT_EOC
 */
struct FAT{
    uint16_t content;
};

struct root_directory{
    char filename[FS_FILENAME_LEN];
    uint32_t size_of_file;
    uint16_t idx_fst_data_blk;
    uint8_t padding[10];
};

struct file_info{
    char filename[FS_FILENAME_LEN];
    int index;
    int used;
    size_t offset;
};

struct superblock *my_super;
struct FAT *my_FAT;
struct root_directory *my_root;
struct file_info my_disk[FS_OPEN_MAX_COUNT];

int fs_mount(const char *diskname)
{
	/* TODO: Phase 1 */
    if (block_disk_open(diskname) == -1){
        fprintf(stderr, "cannot open the target disk\n");
        return -1;
    }

    // section of initialize the super block
    my_super = (struct superblock*)malloc(BLOCK_SIZE);
    if(block_read(0, (void*)my_super) == -1){
        fprintf(stderr, "cannot read from target block\n");
        return -1;
    }
    if (strncmp(my_super->signature, "ECS150FS", 8) != 0) {
        fprintf(stderr, "signature fault\n");
        return -1;
    }
    if (my_super->total_blk != block_disk_count()) {
        fprintf(stderr, "block amount incorrect\n");
        return -1;
    }
    // done initialization of super block

    // section of initialize the FAT
    my_FAT = (struct FAT*)malloc((my_super->num_FAT_blk)*(BLOCK_SIZE));
    for (int i = 0; i < my_super->num_FAT_blk; i++){
        if(block_read(i + 1, (void*)my_FAT + (i*BLOCK_SIZE)) == -1){
            fprintf(stderr, "cannot read from disk.\n");
            return -1;
        }
    }
    // done initialization for FAT

    // section of initialize the root directory
    my_root = (struct root_directory*)malloc(sizeof(struct root_directory) * FS_FILE_MAX_COUNT);
    if(block_read(my_super->num_FAT_blk + 1, (void*)my_root) == -1){
        fprintf(stderr, "cannot read from disk.\n");
        return -1;
    }

    // set every file's status of use in 0 -> unused.
    //for(int i = 0; i < FS_FILE_MAX_COUNT; i++){
    //    my_disk[i].used = 0;
    //}

    return 0;
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
    if(!my_super){
        fprintf(stderr, "there is no super block\n");
        return -1;
    }
    if(block_write(0, (void*)my_super) == -1){
        fprintf(stderr, "fail to write into the block\n");
        return -1;
    }
    for(int i =0; i < my_super->num_FAT_blk; i ++){
        if (block_write(i+1, (void*)my_FAT+(i*BLOCK_SIZE)) == -1){
            fprintf(stderr, "fail to write into the block\n");
            return -1;
        }
    }
    if(block_write(my_super->num_FAT_blk+1, (void*)my_root) == -1){
        fprintf(stderr, "fail to write into the block\n");
        return -1;
    }
    /**
    clearance of the file descriptor array.
    */
    free(my_super);
    free(my_FAT);
    free(my_root);
    block_disk_close();
    return 0;
}

int fs_info(void)
{
	/* TODO: Phase 1 */
    if (!my_super){
        fprintf(stderr, "there is not disk opening\n");
        return -1;
    }
    int free_FAT_blocks = 0;
    int free_root_dir = 0;
    for (int i = 0; i < my_super->data_blk_amount; i++){
        if (my_FAT[i].content == 0){
            free_FAT_blocks++;
        }
    }
    for(int i = 0; i < FS_FILE_MAX_COUNT; i ++){
        if (my_root[i].filename[0] == 0){
            free_root_dir++;
        }
    }
    printf("FS Info:\n");
    printf("total_blk_count=%d\n", my_super->total_blk);
    printf("fat_blk_count=%d\n", my_super->num_FAT_blk);
    printf("rdir_blk=%d\n", my_super->root_idx);
    printf("data_blk=%d\n", my_super->data_start_idx);
    printf("data_blk_count=%d\n", my_super->data_blk_amount);
    printf("fat_free_ratio=%d/%d\n", free_FAT_blocks, my_super->data_blk_amount);    // need to implement
    printf("rdir_free_ratio=%d/128\n", free_root_dir);
    return 0;
}

int fs_create(const char *filename)
{

    if (!my_super){
        fprintf(stderr, "There is not disk opening\n");
        return -1;
    }

    if (filename == NULL){
        fprintf(stderr, "Invalid filename\n");
        return -1;
    }

    if (strlen(filename) > FS_FILENAME_LEN){
        fprintf(stderr, "Filename is too long\n");
        return -1;
    }

    for (int i = 0; i < FS_FILE_MAX_COUNT; i++){
        if (strcmp (my_root[i].filename, filename) == 0) {
            fprintf(stderr, "File already exists\n");
            return -1;
        }
    }

    for (int i = 0; i < FS_FILE_MAX_COUNT; i++){
        if (my_root[i].filename[0] == 0){
            strcpy(my_root[i].filename, filename);
            my_root[i].size_of_file = 0;
            my_root[i].idx_fst_data_blk = FAT_EOC;
            return 0;
        }
    }

    fprintf(stderr, "Root directory already contains %d files\n", FS_FILE_MAX_COUNT);
    return -1;
}

int fs_delete(const char *filename)
{
    if (!my_super){
        fprintf(stderr, "There is not disk opening\n");
        return -1;
    }

    if (filename == NULL){
        fprintf(stderr, "Invalid filename\n");
        return -1;
    }


    for (int i = 0; i < FS_FILE_MAX_COUNT; i++){
        if (strcmp(my_root[i].filename, filename) == 0){
            //printf("%dth trial of fs_file_max_count\n", i);
            for (int j = 0; j < FS_OPEN_MAX_COUNT; j ++){
                if(strcmp(my_disk[j].filename, filename) == 0){
                    fprintf(stderr, "File is currently open\n");
                    return -1;
                }
            }
            //printf("done checking the file descriptor array.\n");
            memset(my_root[i].filename, 0, FS_FILENAME_LEN*(sizeof(char)));
            my_root[i].size_of_file = 0;
            my_root[i].idx_fst_data_blk = FAT_EOC;
            int fat_index = my_root[i].idx_fst_data_blk;

            //printf("entering the loop of fat array\n");
            while (fat_index != FAT_EOC){
                uint16_t next_index = my_FAT[fat_index].content;
                my_FAT[fat_index].content = 0;
                fat_index = next_index;
            }
            //printf("done checking the fay array.\n");
            //my_FAT[fat_index].content = 0;
            return 0;
        }
    }
    fprintf(stderr, "No such file\n");
    return -1;

}

int fs_ls(void)
{
	/* TODO: Phase 2 */
    if(!my_super){
        fprintf(stderr, "no FS is currently opened\n");
        return -1;
    }
    printf("FS Ls:\n");
    for(int i = 0; i < FS_FILE_MAX_COUNT; i++){
        if(my_root[i].filename[0] != 0){
            printf("file: %s, size: %d, data_blk: %d\n",
                   my_root[i].filename, my_root[i].size_of_file,
                   my_root[i].idx_fst_data_blk);
        }
    }
    return 0;
}

/**
 * fs_open - Open a file
 * @filename: File name
 *
 * Open file named @filename for reading and writing, and return the
 * corresponding file descriptor. The file descriptor is a non-negative integer
 * that is used subsequently to access the contents of the file. The file offset
 * of the file descriptor is set to 0 initially (beginning of the file). 
 * 
 * If the
 * same file is opened multiple files, fs_open() must return distinct file
 * descriptors. 
 * 
 * A maximum of %FS_OPEN_MAX_COUNT files can be open
 * simultaneously.
 *
 * Return: -1 if no FS is currently mounted, or if @filename is invalid, or if
 * there is no file named @filename to open, or if there are already
 * %FS_OPEN_MAX_COUNT files currently open. Otherwise, return the file
 * descriptor.
 */
int fs_open(const char *filename)
{
    if (!my_super){
        fprintf(stderr, "There is not disk opening\n");
        return -1;
    }

    if (filename == NULL){
        fprintf(stderr, "Invalid filename\n");
        return -1;
    }

    if (strlen(filename) > FS_FILENAME_LEN){
        fprintf(stderr, "Filename is too long\n");
        return -1;
    }

	for (int i = 0; i < FS_OPEN_MAX_COUNT; i++){
        if(my_disk[i].filename[0] == 0){
            strcpy(my_disk[i].filename, filename);
            // my_disk[i].index = ;
            // my_disk[i].used = 1; 
            return i;
        }
        fprintf(stderr, "Already Max nums of file open\n");
        return -1;
    }


}

/**
 * fs_close - Close a file
 * @fd: File descriptor
 *
 * Close file descriptor @fd.
 *
 * Return: -1 if no FS is currently mounted, or if file descriptor @fd is
 * invalid (out of bounds or not currently open). 0 otherwise.
 */
int fs_close(int fd)
{
	if (!my_super){
        fprintf(stderr, "There is not disk opening\n");
        return -1;
    }

    if (fd >= FS_FILE_MAX_COUNT || fd < 0){
        fprintf(stderr, "Invalid file descriptor\n");
        return -1;
    }

    //File is currently open
    if(my_disk[fd].filename[0] == 0){
        fprintf(stderr, "Invalid file descriptor\n");
        return -1;
    }

    memset(my_disk[fd].filename, 0, FS_FILENAME_LEN*(sizeof(char)));
    return 0;
}

/**
 * fs_stat - Get file status
 * @fd: File descriptor
 *
 * Get the current size of the file pointed by file descriptor @fd.
 *
 * Return: -1 if no FS is currently mounted, of if file descriptor @fd is
 * invalid (out of bounds or not currently open). Otherwise return the current
 * size of file.
 */
int fs_stat(int fd)
{
    if (!my_super){
        fprintf(stderr, "There is not disk opening\n");
        return -1;
    }

    if (fd >= FS_FILE_MAX_COUNT || fd < 0){
        fprintf(stderr, "Invalid file descriptor\n");
        return -1;
    }

    //File is currently open
    if(my_disk[fd].filename[0] == 0){
        fprintf(stderr, "Invalid file descriptor\n");
        return -1;
    }

    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++){
        if (strcmp(my_root[i].filename, my_disk[fd].filename)){
            return my_root[i].size_of_file;
        }
    }
	
}

/**
 * fs_lseek - Set file offset
 * @fd: File descriptor
 * @offset: File offset
 *
 * Set the file offset (used for read and write operations) associated with file
 * descriptor @fd to the argument @offset. To append to a file, one can call
 * fs_lseek(fd, fs_stat(fd));
 *
 * Return: -1 if no FS is currently mounted, or if file descriptor @fd is
 * invalid (i.e., out of bounds, or not currently open), or if @offset is larger
 * than the current file size. 0 otherwise.
 */

int fs_lseek(int fd, size_t offset)
{
    if (!my_super){
        fprintf(stderr, "There is not disk opening\n");
        return -1;
    }

    if (fd >= FS_OPEN_MAX_COUNT || fd < 0){
        fprintf(stderr, "Invalid file descriptor\n");
        return -1;
    }

    if(my_disk[fd].filename[0] == 0){
        fprintf(stderr, "Not currently open\n");
        return -1;
    }

    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++){
        if (strcmp(my_root[i].filename, my_disk[fd].filename)){
            if (offset > my_root[i].size_of_file){
                fprintf(stderr, "Invalid offset\n");
                return -1;
            }  
        }
    }

    my_disk[fd].offset = offset;
    return 0;
    
}


/**
 * fs_write - Write to a file
 * @fd: File descriptor
 * @buf: Data buffer to write in the file
 * @count: Number of bytes of data to be written
 *
 * Attempt to write @count bytes of data from buffer pointer by @buf into the
 * file referenced by file descriptor @fd. It is assumed that @buf holds at
 * least @count bytes.
 *
 * When the function attempts to write past the end of the file, the file is
 * automatically extended to hold the additional bytes. 
 * 
 * If the underlying disk runs out of space while performing a write operation, 
 * fs_write() should write
 * as many bytes as possible. The number of written bytes can therefore be
 * smaller than @count (it can even be 0 if there is no more space on disk).
 *
 * Return: -1 if no FS is currently mounted, or if file descriptor @fd is
 * invalid (out of bounds or not currently open), or if @buf is NULL. Otherwise
 * return the number of bytes actually written.
 */

int fs_write(int fd, void *buf, size_t count)
{
    if (!my_super){
    fprintf(stderr, "There is not disk opening\n");
    return -1;
    }

    if (fd >= FS_FILE_MAX_COUNT || fd < 0){
        fprintf(stderr, "Invalid file descriptor\n");
        return -1;
    }

    //File is currently open
    if(my_disk[fd].filename[0] == 0){
        fprintf(stderr, "Invalid file descriptor\n");
        return -1;
    }
    if (buf == NULL){
        fprintf(stderr, "Buffer is NULL\n");
        return -1;
    } 

    size_t curr_offset = get_offset(fd);
    size_t pos_file = get_file(fd);
    int curr_blk = curr_offset/BLOCK_SIZE;
    int left_in_first_blk = curr_blk * BLOCK_SIZE - curr_offset;
    int num_blk = 0;


    // case1: can be filled in the curr block entirely
    // case2: need more than one block to fill in
    // subcase1: exceed the end of the file, grows the file 
    // subcase2: exceed the max size of the my_disk, fill as many as possible
    
    //can be filled in the curr block entirely 
    
    if (count < left_in_first_blk){
        char bounce_buf[BLOCK_SIZE];
        //get the entire curr block into bounce_buf
        block_read(curr_blk, bounce_buf);

        //merge bounce_buf with buf by the offset
        int start_pos = curr_offset%BLOCK_SIZE;
        memcpy(bounce_buf + start_pos, buf, count);

        //refill the content of curr block with buf
        block_write(curr_blk, bounce_buf);

    }else{
        if ((count-left_in_first_blk)%BLOCK_SIZE != 0){
            num_blk = 1 +(count-left_in_first_blk)/BLOCK_SIZE + 1;
        }else{
            num_blk = 1 +(count-left_in_first_blk)/BLOCK_SIZE;
        }
        int total_blk = my_root[pos_file].size_of_file / BLOCK_SIZE;
        int extended_blk = total_blk - (curr_blk + num_blk);
        int left_in_last_blk = count - num_blk*BLOCK_SIZE;
        char bounce_buf[BLOCK_SIZE*num_blk];
        curr_blk = curr_offset/BLOCK_SIZE;
        //check whether we will exceed the max size of the disk
        
        int free_FAT_blocks = 0;
        int actual_filled_count = 0;
        for (int i = 0; i < my_super->data_blk_amount; i++){
            if (my_FAT[i].content == 0){
                free_FAT_blocks++;
            }
        }
        if (extended_blk > free_FAT_blocks){
            num_blk = free_FAT_blocks;
            actual_filled_count = free_FAT_blocks * BLOCK_SIZE;
        }

        //read entirely content from the blk we should replace with
        for (int i = 0; i < num_blk; i++){
            block_read(curr_blk, bounce_buf);
            curr_blk++;
        }
        //do the replace
        int start_pos = curr_offset%BLOCK_SIZE;
        memcpy(bounce_buf + start_pos, buf, count);

        //refill the content in bounce buffer to the block one by one
        curr_blk = curr_offset/BLOCK_SIZE;
        char content[BLOCK_SIZE];
        
        for (int i = 0; i < num_blk; i++){
            block_write(curr_blk, content);
            curr_blk++;
        }
        //update the file offset
        my_disk[fd].offset = curr_offset + actual_filled_count;
        }
    return count;
}

/**
 * fs_read - Read from a file
 * @fd: File descriptor
 * @buf: Data buffer to be filled with data
 * @count: Number of bytes of data to be read
 *
 * Attempt to read @count bytes of data from the file referenced by file
 * descriptor @fd into buffer pointer by @buf. It is assumed that @buf is large
 * enough to hold at least @count bytes.
 *
 * The number of bytes read can be smaller than @count if there are less than
 * @count bytes until the end of the file (it can even be 0 if the file offset
 * is at the end of the file). The file offset of the file descriptor is
 * implicitly incremented by the number of bytes that were actually read.
 *
 * Return: -1 if no FS is currently mounted, or if file descriptor @fd is
 * invalid (out of bounds or not currently open), or if @buf is NULL. Otherwise
 * return the number of bytes actually read.
 */

int fs_read(int fd, void *buf, size_t count)
{
    printf("Start reading ----!\n");
    if (!my_super){
        fprintf(stderr, "There is not disk opening\n");
        return -1;
    }

    if (fd >= FS_FILE_MAX_COUNT || fd < 0){
        fprintf(stderr, "Invalid file descriptor\n");
        return -1;
    }

    //File is currently open
    if(my_disk[fd].filename[0] == 0){
        fprintf(stderr, "Invalid file descriptor\n");
        return -1;
    }
    if (buf == NULL){
        fprintf(stderr, "Buffer is NULL\n");
        return -1;
    } 
	size_t curr_offset = get_offset(fd);
    int pos_file = get_file(fd);
    int left = my_root[pos_file].size_of_file - curr_offset;
    //not exceed
    if (left < count) count = left;
  
    int curr_blk = curr_offset/BLOCK_SIZE;
    int start_pos = curr_offset%BLOCK_SIZE;

    int left_in_first_blk = BLOCK_SIZE - start_pos;
    int num_blk = 1;
    int left_in_last_blk = 0;
    char bounce_buf[BLOCK_SIZE];

    for (int i = 0; i < FS_FILE_MAX_COUNT; i++){
        if (strcmp(my_root[i].filename, my_disk[fd].filename)){
            if (curr_offset > my_root[i].size_of_file){
                fprintf(stderr, "Invalid offset\n");
                return -1;
            }  
        }
    }

    //only need to read from the first block
    if (left_in_first_blk < count){
        if ((count-left_in_first_blk)%BLOCK_SIZE != 0){
            num_blk = count-left_in_first_blk/BLOCK_SIZE + 1;
        }else{
            num_blk = count-left_in_first_blk/BLOCK_SIZE;
        }
    }else{
        uint16_t curr_index = my_root->idx_fst_data_blk;
        for (int i = 0; i < curr_blk; i++) {
		    if (curr_index == FAT_EOC) {
			    fprintf(stderr, "attempted to exceed end of file chain");
			    return -1;
		    }
		    curr_index = my_FAT[curr_index].content;
	    }
        printf("The curr super data index is %d\n", my_super->data_start_idx);
        
        block_read(my_super->data_start_idx + curr_index, (void*)bounce_buf);
        printf("The number of fat blk is %d\n", my_super->data_start_idx + curr_index);
        printf("--------The bounce buff is%s\n", bounce_buf);
        memcpy((char*)buf, bounce_buf + start_pos, BLOCK_SIZE-start_pos);
        my_disk[fd].offset = curr_offset + count;
        printf("Return line 628 --------!\n");
        return count;
    }

    left_in_last_blk = count - left_in_first_blk - (num_blk - 2)*BLOCK_SIZE;
   
    // for (int i = 0; i < num_blk; i++){
    //     block_read(my_super->num_FAT_blk + 2 + start_blk, bounce_buf);
    //     printf("---------The bounce buff is%s\n", bounce_buf );
    //     if (i == 0){
    //         memcpy((char*)buf, bounce_buf+start_pos, BLOCK_SIZE-start_pos);
    //     }
    //     else if (i == num_blk - 1){
    //         memcpy((char*)buf, bounce_buf, left_in_last_blk);
    //     }
    //     else {
    //         memcpy((char*)buf, bounce_buf, BLOCK_SIZE);
    //     }
    //     start_blk++; 
    // }
    //update file offset
    my_disk[fd].offset = curr_offset + count;
    printf("Return line 651 ----!\n");
    return count;
}

size_t get_offset(int fd){
    return my_disk[fd].offset;
}

int get_file(int fd){
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++){
        if (strcmp(my_root[i].filename, my_disk[fd].filename)){
            return i;
        }
    }
}

