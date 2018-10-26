#include "udp.h"
#include "mfs.h"

// Global variable for connection information
int myport;
int connection;
struct sockaddr_in addr, addr2;


// Takes hostname/port and finds server exporting file system. 
// Return 0 if success, -1 if failure
int MFS_Init(char *hostname, int port) {
	myport = UDP_Open(9009);
	assert(myport > -1);
	connection = UDP_FillSockAddr(&addr, hostname, port); //contact server at specified port
	printf("Hostname: %s || port: %d\n", hostname, port);
    assert(connection == 0);
	return 0;
}


// Looks at inode at pinum for entry name, 
// Returns inode number of entry or -1 if not found
int MFS_Lookup(int pinum, char *name) {
	// lookup pinum name
	char message[4096];
	char reply[4096];

	printf("LOOKUP\n");
	sprintf(message, "lookup %d %s", pinum, name);
    connection = UDP_Write(myport, &addr, message, 4096); //write message to server@specified-port
    printf("CLIENT:: sent message (%d)\n", connection);
    if (connection > 0) {
		connection = UDP_Read(myport, &addr2, reply, 4096); //read message from ...
		printf("CLIENT:: read %d bytes (message: '%s')\n", connection, reply);
		int replyint = atoi(reply);
		return replyint;
    }
	return -1;
}


// Returns MFS_Stat_t linked to by inum. 
// Returns 0 if success, -1 if failure (inum does not exist).
int MFS_Stat(int inum, MFS_Stat_t *m) {
	// stat inum
	// RETURNS BUFFER
	char message[4096];
	char reply[4096];
	printf("STAT\n");
	sprintf(message, "stat %d", inum);
    connection = UDP_Write(myport, &addr, message, 4096); //write message to server@specified-port
    printf("CLIENT:: sent message (%d)\n", connection);
    if (connection > 0) {
		connection = UDP_Read(myport, &addr2, reply, 4096); //read message from ...
		printf("CLIENT:: read %d bytes (message: '%s')\n", connection, reply);
		char *chunk1 = strtok(reply, " ");
		printf("chunk1: %s\n", chunk1);
		int replyint = atoi(chunk1);
		if (replyint > -1) {
			char *chunk2 = strtok(NULL, " ");
			printf("chunk2: %s\n", chunk2);
			char *chunk3 = strtok(NULL, " ");
			printf("chunk3: %s\n", chunk3);
			char *chunk4 = strtok(NULL, " ");
			printf("chunk4: %s\n", chunk4);
	 		int replyint = atoi(chunk1);
			m->type = atoi(chunk2);
			m->size = atoi(chunk3);
			m->blocks = atoi(chunk4);
		}
		return replyint;
    }
	return -1;
}


// Writes block of 4096 bytes at block# block in inode inum. 
// Returns 0 if success, -1 if failure (invalid inum, invalid block, directory inum)
int MFS_Write(int inum, char *buffer, int block) {
	// write inum block [data]
	char message[4096 * 2];
	char reply[4096];
	printf("WRITE\n");
	sprintf(message, "write %d %d %s", inum, block, buffer);
	printf("SENDING...\n");
    connection = UDP_Write(myport, &addr, message, 4096 * 2); //write message to server@specified-port
    printf("CLIENT:: sent message (%d)\n", connection);
    if (connection > 0) {
		connection = UDP_Read(myport, &addr2, reply, 4096); //read message from ...
		printf("CLIENT:: read %d bytes (message: '%s')\n", connection, reply);
		int replyint = atoi(reply);
		return replyint;
    }
	return -1;
}


// Reads block# block into buffer at inode inum. 
// Returns 0 if success, -1 if failure (invalid inum, invalid block)
int MFS_Read(int inum, char *buffer, int block) {
	// read inum block
	// RETURNS BUFFER
	char message[4096];
	char reply[4096 * 2];
	printf("READ\n");
	sprintf(message, "read %d %d", inum, block);
    connection = UDP_Write(myport, &addr, message, 4096); //write message to server@specified-port
    printf("CLIENT:: sent message (%d)\n", connection);
    if (connection > 0) {
		connection = UDP_Read(myport, &addr2, reply, 4096 * 2); //read message from ...
		printf("CLIENT:: read %d bytes (message: '%s')\n", connection, reply);
		char *chunk1 = strtok(reply, " ");
		int replyint = atoi(chunk1);
		if (replyint != -1) {
			char *chunk2 = strtok(NULL, " ");
			printf("chunk2: %s\n", chunk2);
			memcpy(buffer, chunk2, MFS_BLOCK_SIZE);
		}
		return replyint;
    }
	return -1;
}


// Creates new file/directory in inode pinum with name name. 
// Returns 0 if success, -1 if failure (pinum does not exist)
int MFS_Creat(int pinum, int type, char *name) {
	// creat pinum type [name]
	char message[4096];
	char reply[4096];
	printf("CREAT\n");
	sprintf(message, "creat %d %d %s", pinum, type, name);
    connection = UDP_Write(myport, &addr, message, 4096); //write message to server@specified-port
    printf("CLIENT:: sent message (%d)\n", connection);
    if (connection > 0) {
		connection = UDP_Read(myport, &addr2, reply, 4096); //read message from ...
		printf("CLIENT:: read %d bytes (message: '%s')\n", connection, reply);
		int replyint = atoi(reply);
		return replyint;
    }
	return -1;
}


// Removes file/directory name from directory at pinum. 
// Returns 0 if success, -1 if failure (invalid pinum, pinum is not directory, removed directory is not empty)
int MFS_Unlink(int pinum, char *name) {
	// unlink pinum [name]
	char message[4096];
	char reply[4096];
	printf("UNLINK\n");
	sprintf(message, "unlink %d %s", pinum, name);
    connection = UDP_Write(myport, &addr, message, 4096); //write message to server@specified-port
    printf("CLIENT:: sent message (%d)\n", connection);
    if (connection > 0) {
		connection = UDP_Read(myport, &addr2, reply, 4096); //read message from ...
		printf("CLIENT:: read %d bytes (message: '%s')\n", connection, reply);
		int replyint = atoi(reply);
		return replyint;
    }
	return -1;
}

