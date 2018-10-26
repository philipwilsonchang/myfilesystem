#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "udp.h"
#include "mfs.h"

#define NUM_INODES (4096)
#define NUM_BLOCKS (4096)
#define INODE_SIZE (52)
#define BLOCK_SIZE (4096)


#define FIRST_INODE (1024)
#define INODE_BITMAP_START (0)
#define BLOCK_BITMAP_START (512)
#define INODE_START (1024)
#define BLOCK_START (214016)

#define INODE_OFFSET_TYPE (0)
#define INODE_OFFSET_SIZE (4)
#define INODE_OFFSET_NUM_B (8)
#define INODE_OFFSET_PTR (12)

#define BUFFER_SIZE (4096)
#define FS_SIZE (16978944)
// #define MFS_DIRECTORY    (0) // defined in mfs.h
// #define MFS_REGULAR_FILE (1)

/***************
Filesystem Structure:
Bytes 0-511: Inode bitmap
Bytes 512-1023: Data block bitmap
Bytes 1024-214015: Inodes (212992 bytes)
Bytes 214016-16991231: Data blocks
Total size (max): 16.9 MB
***************/

/***************
Bitmap Structure:
char's of 1 byte each - bits accessed via bit manipulation
***************/

/***************
Inode Structure (File):
Byte 0-3: type (type int, 0 = dir, 1 = file)
Byte 4-7: size in bytes (type int)
Byte 8-11: num of blocks (type int)
Byte 12-51: 	If type 1, pointer to up to 10 blocks (type int, indicates byte number of block)
			If type 0, pointer to up to 10 blocks pointing to other inodes (type int, indicates block number)
NOTE: inode name is stored in blocks linked to directory inodes - 4 byte inode number + 252 bytes of char
NOTE: if block pointer is -1, it is unusued
NOTE: blocks are numbered 0-9
Total size: 52 bytes
***************/

int fs_creat(int pinum, int type, char *name);

int fs = -1;

// Checks if inum inode is valid in bitmap
// Returns 0 if free inum, 1 if occupied
int valid_inum(int inum) {
	// Seek byte number of inum in bitmap
	lseek(fs, INODE_BITMAP_START + (inum / 8), SEEK_SET);
	char *ichunk = malloc(sizeof(char));
	int bytesread = read(fs, ichunk, 1);
	// Get bit number within byte of inum
	int bitnum = inum % 8;
	return (*ichunk >> bitnum) & 1;
}

// Check if block number blocknum is valid in bitmap
// Return 0 if free, 1 if occupied
int valid_block(int blocknum) {
	// Seek byte number of inum in bitmap
	lseek(fs, BLOCK_BITMAP_START + blocknum / 8, SEEK_SET);
	char *ichunk = malloc(sizeof(char));
	read(fs, (void *) ichunk, 1);

	// Get bit number within byte of inum
	int bitnum = blocknum % 8;
	return (*ichunk >> bitnum) & 1;
}

// Set bitmap inode inum bit to value
// Return 0 if success, -1 if failure
int set_inode_bitmap(int inum, int value) {
	// Get value of corresponding byte in bitmap
	lseek(fs, INODE_BITMAP_START + (inum / 8), SEEK_SET);
	char *ichunk = malloc(sizeof(char));
	int status = read(fs, ichunk, 1);
	if (status < 0) {
		return -1;
	}

	// Reset bit
	int bitnum = inum % 8;
	char mask = 0x01 << bitnum;
	if (value == 0) {
		mask ^= 0xff;
		*ichunk = *ichunk & mask;
	}
	// Set bit
	else {
		*ichunk = *ichunk | mask;
	}

	// Write new byte to fs
	lseek(fs, INODE_BITMAP_START + (inum / 8), SEEK_SET);
	status = write(fs, ichunk, 1);
	if (status < 0) {
		return -1;
	}
	return 0;
}

// Set bitmap inode inum bit to value
int set_block_bitmap(int inum, int value) {
	// Get value of corresponding byte in bitmap
	lseek(fs, BLOCK_BITMAP_START + (inum / 8), SEEK_SET);
	char *ichunk = malloc(sizeof(char));
	int status = read(fs, ichunk, 1);
	if (status < 0) {
		return -1;
	}

	// Reset bit
	int bitnum = inum % 8;
	char mask = 0x01 << bitnum;
	if (value == 0) {
		mask ^= 0xff;
		*ichunk = *ichunk & mask;
	}
	// Set bit
	else {
		*ichunk = *ichunk | mask;
	}

	// Write new byte to fs
	lseek(fs, BLOCK_BITMAP_START + (inum / 8), SEEK_SET);
	status = write(fs, ichunk, 1);
	if (status < 0) {
		return -1;
	}
	return 0;
}

// Checks if inum inode is of type directory
// NOTE: Does not check if inode inum is valid!
// Returns 0 if success, -1 if failure
int is_directory(int inum) {
	lseek(fs, INODE_START + (inum * INODE_SIZE), SEEK_SET);
	char *byte = malloc(sizeof(int));
	int status = read(fs, byte, 4);
	int *intbyte = (int *) byte;
	if (*intbyte == 0) {
		return 0;
	}
	else {
		return -1;
	}
}

// Searches through block bitmap for first free block
// Returns block number of free block, -1 if none found or error
int find_free_block() {
	for (int i = 0; i < NUM_BLOCKS; i++) {
		if (valid_block(i) == 0) {
			return i;
		}
	}
	return -1;
}

// Searches through inode bitmap for first free inode
// Returns inode number of free inode, -1 if none found or error
int find_free_inode() {
	for (int i = 0; i < NUM_INODES; i++) {
		if (valid_inum(i) == 0) {
			return i;
		}
	}
	return -1;
}

// Resets file system image
// Returns 0 if success, -1 if failure
int reset_fs() {
	int status;

	// Check for failure
	if (fs == -1) {
		return -1;
	}
	
	// Set bitmaps to 0
	for (int i = 0; i < NUM_INODES ; i++) {
		set_inode_bitmap(i,0);
		set_block_bitmap(i,0);
	}
	// Write 0 to end of filesystem to set file size
	lseek(fs, FS_SIZE-1, SEEK_SET);
	status = write(fs, "a", 1);
	lseek(fs, 0, SEEK_SET);

	// Set first inode as root directory
	set_inode_bitmap(0, 1);
	char *intwriter = malloc(sizeof(int));
	int *wrapper = (int *)intwriter;
	*wrapper = 0;
	lseek(fs, FIRST_INODE, SEEK_SET);
	status = write(fs, intwriter, 1);	// write type = 0
	write(fs, intwriter, 4); // write size = 0
	write(fs, intwriter, 4); // write num blocks = 0
	
	// Fill data block pointers with -1 to indicate unused
	*wrapper = -1;
	for (int i = 0; i < 10; i++) {
		write(fs, intwriter, 4);
	}
	fsync(fs);


	// Write "." entry to root directory
	int newblockid = find_free_block();
	set_block_bitmap(newblockid, 1);
	lseek(fs, BLOCK_START + (newblockid * BLOCK_SIZE), SEEK_SET);
	*wrapper = 0;
	status = write(fs, intwriter, 4);
	status = write(fs, ".", 252);
	
	// Link new inode to pinum inode
	*wrapper = newblockid;
	lseek(fs, INODE_START + (0 * INODE_SIZE) + INODE_OFFSET_PTR + (0 * sizeof(int)), SEEK_SET);
	status = write(fs, intwriter, 4);
	
	// Update number of blocks field in pinum inode
	lseek(fs, INODE_START + (0 * INODE_SIZE) + INODE_OFFSET_NUM_B, SEEK_SET);
	status = read(fs, intwriter, 4);
	*wrapper = 1;
	lseek(fs, INODE_START + (0 * INODE_SIZE) + INODE_OFFSET_NUM_B, SEEK_SET);
	status = write(fs, intwriter, 4); 

	// Update size fields
	lseek(fs, INODE_START + (0 * INODE_SIZE) + INODE_OFFSET_SIZE, SEEK_SET);
	status = read(fs, intwriter, 4);
	*wrapper += 256;
	lseek(fs, INODE_START + (0 * INODE_SIZE) + INODE_OFFSET_SIZE, SEEK_SET);
	status = write(fs, intwriter, 4); 

	fsync(fs);

	// Write ".." entry to root directory
	newblockid = find_free_block();
	set_block_bitmap(newblockid, 1);
	lseek(fs, BLOCK_START + (newblockid * BLOCK_SIZE), SEEK_SET);
	*wrapper = 0;
	status = write(fs, intwriter, 4);
	status = write(fs, "..", 252);
	
	// Link new inode to pinum inode
	*wrapper = newblockid;
	lseek(fs, INODE_START + (0 * INODE_SIZE) + INODE_OFFSET_PTR + (1 * sizeof(int)), SEEK_SET);
	status = write(fs, intwriter, 4);
	
	// Update number of blocks field in pinum inode
	lseek(fs, INODE_START + (0 * INODE_SIZE) + INODE_OFFSET_NUM_B, SEEK_SET);
	status = read(fs, intwriter, 4);
	*wrapper = 1;
	lseek(fs, INODE_START + (0 * INODE_SIZE) + INODE_OFFSET_NUM_B, SEEK_SET);
	status = write(fs, intwriter, 4); 

	// Update size fields
	lseek(fs, INODE_START + (0 * INODE_SIZE) + INODE_OFFSET_SIZE, SEEK_SET);
	status = read(fs, intwriter, 4);
	*wrapper += 256;
	lseek(fs, INODE_START + (0 * INODE_SIZE) + INODE_OFFSET_SIZE, SEEK_SET);
	status = write(fs, intwriter, 4); 

	// Flush to disk
	return fsync(fs);
}

// Loads filesystem image file at filename
// If filename not found, creates new filesystem image file 
// Returns 0 if success, -1 if failure
int load_fs(char *filename) {	
	// Check for existence 
	int status = access(filename, F_OK);

	// Open fs and set global pointer
	fs = open(filename, O_RDWR|O_CREAT, 0666);

	// Reset fs if new file created
	if (status < 0) {
		reset_fs();
	}
	// printf("errno: %d\n", errno);
	
	// Check if open fs succeeded
	if (fs == -1) {
		return -1;
	}
	else {
		return 0;
	}
}

// Looks at inode at pinum for entry name, 
// Returns inode number of entry or -1 if not found
int fs_lookup(int pinum, char *name) {
	// Check for valid pinum and if pinum is directory
	if ((pinum < 0) || (pinum > NUM_INODES - 1)) {
		return -1;
	}
	if ((valid_inum(pinum) == 0) || (is_directory(pinum) == -1)) {
		return -1;
	}
	// Check all child inodes for name in data blocks (skip directories)
	int status;
	char *intbuffer = malloc(sizeof(int));
	int *blockid = (int *) intbuffer;
	char *inodeintbuffer = malloc(sizeof(int));
	int *inodeid = (int *) inodeintbuffer;
	char *namebuffer = malloc(sizeof(char) * 252);
	for (int i = 0; i < 10; i++) {
		lseek(fs, INODE_START + (pinum * INODE_SIZE) + INODE_OFFSET_PTR + (i * sizeof(int)), SEEK_SET);
		status = read(fs, intbuffer, sizeof(int));
		if (*blockid != -1) {
			lseek(fs, BLOCK_START + (*blockid * BLOCK_SIZE), SEEK_SET);
			status = read(fs, inodeintbuffer, sizeof(int));
			status = read(fs, namebuffer, sizeof(char) * 252);
			printf("Namebuffer: %s\n", namebuffer);
			// If found, return child inode number
			if (strcmp(namebuffer, name) == 0) {
				return *inodeid;
			}
		}
	}
	// If not found, return -1
	return -1;
}

// Returns MFS_Stat_t linked to by inum. 
// Returns 0 if success, -1 if failure (inum does not exist).
int fs_stat(int inum, int *stat_type, int *stat_size, int *stat_blocks) {
	// Check for valid inum
	if ((inum < 0) || (inum > NUM_INODES - 1)) {
		return -1;
	}
	if (valid_inum(inum) == 0) {
		return -1;
	}

	// Fill MFS_Stat_t object using inode at inum
	char *intreader = malloc(sizeof(int));
	int *wrapper = (int *) intreader;
	
	lseek(fs, INODE_START + (inum * INODE_SIZE) + INODE_OFFSET_TYPE, SEEK_SET);
	read(fs, intreader, sizeof(int));
	*stat_type = *wrapper;
	// printf("type: %d\n", *wrapper);
	
	lseek(fs, INODE_START + (inum * INODE_SIZE) + INODE_OFFSET_SIZE, SEEK_SET);
	read(fs, intreader, sizeof(int));
	*stat_size = *wrapper;
	// printf("size: %d\n", *wrapper);

	lseek(fs, INODE_START + (inum * INODE_SIZE) + INODE_OFFSET_NUM_B, SEEK_SET);
	read(fs, intreader, sizeof(int));
	*stat_blocks = *wrapper;
	// printf("blocks: %d\n", *wrapper);

	// printf("m->type: %d\n", m->type);
	// printf("m->size: %d\n", m->size);
	// printf("m->blocks: %d\n", m->blocks);
	return 0;
}

// Writes block of 4096 bytes at block# block in inode inum. 
// Returns 0 if success, -1 if failure (invalid inum, invalid block, directory inum)
int fs_write(int inum, char *buffer, int block) {
	// Check for valid inum and valid file and valid block
	if ((inum < 0) || (inum > NUM_INODES - 1)) {
		printf("Real basic\n");
		return -1;
	}
	if ((valid_inum(inum) == 0) || (is_directory(inum) == 0)) {
		printf("Basic\n");
		return -1;
	}
	// Check for valid block entry in inum inode
	char *reader = malloc(sizeof(int));
	int *blockstatus = (int *) reader;
	lseek(fs, INODE_START + (inum * INODE_SIZE) + INODE_OFFSET_PTR + (sizeof(int) * block), SEEK_SET);
	int status = read(fs, reader, sizeof(int));
	if (*blockstatus != -1) {
		printf("Failed everywhere\n");
		return -1;
	}

	// Create new block (search bitmap for free block)
	int newblockid = find_free_block();
	set_block_bitmap(newblockid, 1);

	// Read buffer into block
	lseek(fs, BLOCK_START + (newblockid * BLOCK_SIZE), SEEK_SET);
	status = write(fs, buffer, BUFFER_SIZE);
	if (status < 0) {
		printf("Failed here\n");
		return -1;
	}
	// Link block to inum inode
	lseek(fs, INODE_START + (inum * INODE_SIZE) + INODE_OFFSET_PTR + (sizeof(int) * block), SEEK_SET);
	status = write(fs, (void *) &newblockid, sizeof(int));
	if (status < 0) {
		printf("Failed there\n");
		return -1;
	}

	// Update inum inode metadata
	lseek(fs, INODE_START + (inum * INODE_SIZE) + INODE_OFFSET_NUM_B, SEEK_SET);
	status = read(fs, reader, sizeof(int));
	printf("Old num-b: %d\n", *blockstatus);
	*blockstatus += 1;
	printf("New num-b: %d\n", *blockstatus);
	lseek(fs, INODE_START + (inum * INODE_SIZE) + INODE_OFFSET_NUM_B, SEEK_SET);
	status = write(fs, reader, sizeof(int));

	lseek(fs, INODE_START + (inum * INODE_SIZE) + INODE_OFFSET_SIZE, SEEK_SET);
	status = read(fs, reader, sizeof(int));
	printf("Old size: %d\n", *blockstatus);
	*blockstatus += BLOCK_SIZE;
	printf("New size: %d\n", *blockstatus);
	lseek(fs, INODE_START + (inum * INODE_SIZE) + INODE_OFFSET_SIZE, SEEK_SET);
	status = write(fs, reader, sizeof(int));


	return 0;
}

// Reads block# block into buffer at inode inum. If inum is directory, return MFS_DirEnt_t in buffer
// Returns 0 if success, -1 if failure (invalid inum, invalid block)
int fs_read(int inum, char *buffer, int block) {
	// Check for valid inum and block number
	if ((inum < 0) || (inum > NUM_INODES - 1)) {
		return -1;
	}
	if (valid_inum(inum) == 0) {
		return -1;
	}
	if ((block < 0) || (block > 9)) {
		return -1;
	}
	// Check for valid block
	int status;
	char *readint = malloc(sizeof(int));
	int *wrapper = (int *) readint;
	lseek(fs, INODE_START + (inum * INODE_SIZE) + INODE_OFFSET_PTR + (block * sizeof(int)), SEEK_SET);
	status = read(fs, readint, sizeof(int));
	if (*wrapper == -1) {
		return -1;
	}

	if (valid_block(*wrapper) == 0) {
		return -1;
	}

	// Read block into buffer
	lseek(fs, BLOCK_START + (*wrapper * BLOCK_SIZE), SEEK_SET);
	status = read(fs, buffer, BLOCK_SIZE);

	return 0;
}

// Creates new file/directory in inode pinum with name name. 
// Returns 0 if success, -1 if failure (pinum does not exist)
int fs_creat(int pinum, int type, char *name) {
	// Check if pinum is valid in bitmap and is a directory
	if ((pinum < 0) || (pinum > NUM_INODES - 1)) {
		return -1;
	}
	if ((valid_inum(pinum) == 0) || (is_directory(pinum) == -1)) {
		return -1;
	}

	// Find free data block entry in pinum
	int status;
	char *readint = malloc(sizeof(int));
	int *wrapper = (int *) readint;
	int free_entry = -1;
	for (int i = 0; i < 10; i++) {
		lseek(fs, INODE_START + (pinum * INODE_SIZE) + INODE_OFFSET_PTR + (i * sizeof(int)), SEEK_SET);
		status = read(fs, readint, sizeof(int));
		if (*wrapper == -1) {
			free_entry = i;
			break;
		}
	}
	printf("FREE ENTRY AT: %d\n", free_entry);
	
	// Check if free data block entry was found in pinum
	if (free_entry == -1) {
		return -1;
	}

	// Create new inode for new file/directory
	int newinum = find_free_inode();
	set_inode_bitmap(newinum, 1);
	lseek(fs, INODE_START + (newinum * INODE_SIZE), SEEK_SET);
	char *intwriter = malloc(sizeof(int));
	wrapper = (int *)intwriter;
	*wrapper = type;
	status = write(fs, intwriter, 4);	// write type
	*wrapper = 0;
	lseek(fs, INODE_START + (newinum * INODE_SIZE) + INODE_OFFSET_SIZE, SEEK_SET);
	status = write(fs, intwriter, 4); // write size = 0
	*wrapper = 0;
	lseek(fs, INODE_START + (newinum * INODE_SIZE) + INODE_OFFSET_NUM_B, SEEK_SET);
	status = write(fs, intwriter, 4); // write num blocks = 0
	
	// Fill data block pointers of new inode with -1 to indicate unused
	*wrapper = -1;
	for (int i = 0; i < 10; i++) {
		write(fs, intwriter, 4);
	}
	
	// Create new file/directory name entry in new data block (search bitmap for free block)
	int newblockid = find_free_block();
	set_block_bitmap(newblockid, 1);
	lseek(fs, BLOCK_START + (newblockid * BLOCK_SIZE), SEEK_SET);
	*wrapper = newinum;
	status = write(fs, intwriter, 4);
	status = write(fs, name, 252);

	// Link inode-name data block to pinum inode
	*wrapper = newblockid;
	lseek(fs, INODE_START + (pinum * INODE_SIZE) + INODE_OFFSET_PTR + (free_entry * sizeof(int)), SEEK_SET);
	status = write(fs, intwriter, 4);

	// If new inode is directory, add "." and ".." directories to data blocks of new inode
	if (type == 0) {
		// Add "." and ".." directories in new inode
		newblockid = find_free_block();
		set_block_bitmap(newblockid, 1);
		lseek(fs, BLOCK_START + (newblockid * BLOCK_SIZE), SEEK_SET);
		*wrapper = newinum;
		status = write(fs, intwriter, 4);
		status = write(fs, ".", 252);

		newblockid = find_free_block();
		set_block_bitmap(newblockid, 1);
		lseek(fs, BLOCK_START + (newblockid * BLOCK_SIZE), SEEK_SET);
		*wrapper = pinum;
		status = write(fs, intwriter, 4);
		status = write(fs, "..", 252);

		// Update new inode metadata
		lseek(fs, INODE_START + (newinum * INODE_SIZE), SEEK_SET);
		*wrapper = type;
		status = write(fs, intwriter, 4);	// write type
		*wrapper = 512;
		status = write(fs, intwriter, 4); // write size = 512
		*wrapper = 1;
		status = write(fs, intwriter, 4); // write num blocks = 1
	}
	
	// Update number of blocks field in pinum inode
	// lseek(fs, INODE_START + (pinum * INODE_SIZE) + INODE_OFFSET_NUM_B, SEEK_SET);
	// status = read(fs, intwriter, 4);
	// *wrapper += 1;
	// lseek(fs, INODE_START + (pinum * INODE_SIZE) + INODE_OFFSET_NUM_B, SEEK_SET);
	// status = write(fs, intwriter, 4); 

	// Update size of pinum inode
	lseek(fs, INODE_START + (pinum * INODE_SIZE) + INODE_OFFSET_SIZE, SEEK_SET);
	status = read(fs, intwriter, 4);
	*wrapper += 256;
	lseek(fs, INODE_START + (pinum * INODE_SIZE) + INODE_OFFSET_SIZE, SEEK_SET);
	status = write(fs, intwriter, 4);

	return 0;
}

// Removes file/directory name from directory at pinum. Does not delete any blocks.
// Returns 0 if success, -1 if failure (invalid pinum, pinum is not directory, removed directory is not empty)
int fs_unlink(int pinum, char *name) {
	// Check if pinum is valid in bitmap and if pinum is directory
	if ((pinum < 0) || (pinum > NUM_INODES - 1)) {
		return -1;
	}
	if ((valid_inum(pinum) == 0) || (is_directory(pinum) == -1)) {
		return -1;
	}
	// Search for name (if not found, return 0)
	int blockid = fs_lookup(pinum, name);
	lseek(fs, BLOCK_START + (blockid * BLOCK_SIZE), SEEK_SET);
	if (blockid == -1) {
		return 0;
	}

	// Get inum associated with name from blockid
	char *intbuffer = malloc(sizeof(int));
	int *inum = (int *) intbuffer;
	int status = read(fs, intbuffer, sizeof(int));

	// Check if name's inode is empty directory
	char *intreader = malloc(sizeof(int));
	int *numblocks = (int *) intreader;
	if (is_directory(*inum) == 0) {
		lseek(fs, INODE_START + (*inum * INODE_SIZE) + INODE_OFFSET_NUM_B, SEEK_SET);
		status = read(fs, intreader, sizeof(int));
		if (*numblocks > 2) {
			return -1;
		}
	}
	
	// Remove inode inum from inode map
	set_inode_bitmap(*inum, 0);

	// Remove inode entry from directory
	for (int i = 0; i < 10; i++) {
		lseek(fs, INODE_START + (pinum * INODE_SIZE) + INODE_OFFSET_PTR + (i * sizeof(int)), SEEK_SET);
		status = read(fs, intbuffer, sizeof(int));
		if (*inum == blockid) {
			printf("Killing entry %d\n", i);
			lseek(fs, INODE_START + (pinum * INODE_SIZE) + INODE_OFFSET_PTR + (i * sizeof(int)), SEEK_SET);
			*numblocks = -1;
			status = write(fs, intreader, sizeof(int));
			break;
		}
	}

	// Adjust pinum metrics
	lseek(fs, INODE_START + (pinum * INODE_SIZE) + INODE_OFFSET_SIZE, SEEK_SET);
	status = read(fs, intreader, sizeof(int));
	printf("Old size: %d\n", *numblocks);
	*numblocks -= 256;
	printf("New size: %d\n", *numblocks);
	lseek(fs, INODE_START + (pinum * INODE_SIZE) + INODE_OFFSET_SIZE, SEEK_SET);
	status = write(fs, intreader, sizeof(int));

	return 0;
}


// Command parser 
// Takes command string from client and executes correct subroutine
// Command string will be in format: "[command] [arg1] [arg2] [arg3]"
int parser(char *command, int *stat1, int *stat2, int *stat3, int *is_read, char *read_buffer) {
	char *cmd, *arg1, *arg2, *arg3;
	int pinum, inum, type, block, result;
	cmd = strtok(command, " ");
	arg1 = strtok(NULL, " ");
	arg2 = strtok(NULL, " ");
	arg3 = strtok(NULL, "");

	// lookup pinum name
	if (strcmp(cmd, "lookup") == 0) {
		printf("lookup!\n");
		pinum = atoi(arg1);
		result = fs_lookup(pinum, arg2);
		return result;
	}
	// stat inum
	// RETURNS BUFFER
	else if (strcmp(cmd, "stat") == 0) {
		printf("stat!\n");
		inum = atoi(arg1);
		result = fs_stat(inum, stat1, stat2, stat3);
		printf("parser stat.type: %d\n", *stat1);
		printf("parser stat.size: %d\n", *stat2);
		printf("parser stat.blocks: %d\n", *stat3);
		return result;
	}
	// write inum block [data]
	else if (strcmp(cmd, "write") == 0) {
		printf("write!\n");
		inum = atoi(arg1);
		block = atoi(arg2);
		result = fs_write(inum, arg3, block);
		return result;
	}
	// read inum block
	// RETURNS BUFFER
	else if (strcmp(cmd, "read") == 0) {
		printf("read!\n");
		*is_read = 0;
		inum = atoi(arg1);
		block = atoi(arg2);
		result = fs_read(inum, read_buffer, block);
		return result;
	}
	// creat pinum type [name]
	else if (strcmp(cmd, "creat") == 0) {
		printf("creat!\n");
		pinum = atoi(arg1);
		type = atoi(arg2);
		result = fs_creat(pinum, type, arg3);
		return result;
	}
	// unlink pinum [name]
	else if (strcmp(cmd, "unlink") == 0) {
		printf("unlink!\n");
		pinum = atoi(arg1);
		result = fs_unlink(pinum, arg2);
		return result;
	}
	else {
		printf("Invalid command received\n");
		return -1;
	}
}

// Main server code
int main(int argc, char *argv[]) {
	// Catch improper starting
	if (argc < 3) {
		printf("Usage: server [port-number] [file-system-image]\n");
		exit(1);
	}

	// Grab file system image
	load_fs(argv[2]);

	printf("First 8 bits:\n");
	for (int i = 0; i < 8; i++) {
		printf("Bit %d: %d\n", i, valid_inum(i));
	}

	// Setup UDP server
	int portid = atoi(argv[1]);
	int comms = UDP_Open(portid);
	assert(comms > -1);

	printf("Listening...\n");

	// Listen for UDP requests
	while (1) {
		struct sockaddr_in s;
		char msg[BUFFER_SIZE * 2];
		int stat1 = -1;
		int stat2 = -1;
		int stat3 = -1;
		int is_read = -1;
		char *return_read_buffer = malloc(BUFFER_SIZE);
		char reply[BUFFER_SIZE * 2];
		int rxStatus = UDP_Read(comms, &s, msg, BUFFER_SIZE * 2);
		// Parse request and send reply
		if (rxStatus > 0) {
			printf("Received %d bytes || Message: '%s'\n", rxStatus, msg);
			// Parse commmand,
			int result = parser(msg, &stat1, &stat2, &stat3, &is_read, return_read_buffer);
			// Reply consists of integer return of function, followed by buffer if buffer is supposed to be returned
			if (stat1 != -1) {
				printf("Replying via stat!\n");
				sprintf(reply, "%d %d %d %d", result, stat1, stat2, stat3);
			}
			else if (is_read != -1) {
				printf("Replying via read!\n");
				sprintf(reply, "%d %s", result, return_read_buffer);
			}
			else {
				printf("Replying via anything else\n");
				sprintf(reply, "%d", result);
			}
			fsync(fs);
			rxStatus = UDP_Write(comms, &s, reply, BUFFER_SIZE * 2);
		}
	}

	return 0;
}


