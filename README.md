# My File System

## Features
  - A UDP file server and corresponding client library in C implemented to learn principles of file system design
  - Implemented support for bitmaps, inodes, inode maps, data blocks, directories, variable file sizes, and idempotent behavior (crash resistant)

## Instructions
	- Compile with:
		$ make
	- Recompile via:
		$ make clean
		$ make
    
## Bugs
	- Incomplete directory structure implementation
	- Unlinking behavior bizarre - unlinks inodes earlier than found inodes with correct name
	- Partially working timeouts - no retrying function implemented
