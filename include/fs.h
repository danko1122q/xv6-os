#ifndef FS_H
#define FS_H

#include "param.h"
#include "types.h"

#define ROOTINO 1

// Disk layout:
// [ boot block | super block | log | inode blocks | free bit map | data blocks ]
//
// The super block describes the file system structure and is computed by mkfs
struct superblock {
	uint size;	 // Total size of file system image (blocks)
	uint nblocks;	 // Number of data blocks
	uint ninodes;	 // Number of inodes
	uint nlog;	 // Number of log blocks
	uint logstart;	 // Block number of first log block
	uint inodestart; // Block number of first inode block
	uint bmapstart;	 // Block number of first free map block
};

// Block addressing structure:
// - Direct blocks: NDIRECT (10 blocks) 
// - Single indirect: NINDIRECT blocks (512 blocks with BSIZE=2048)
// - Double indirect: NINDIRECT * NINDIRECT blocks (262,144 blocks)
#define NDIRECT 10
#define NINDIRECT (BSIZE / sizeof(uint))
#define NDINDIRECT (NINDIRECT * NINDIRECT)
#define MAXFILE (NDIRECT + NINDIRECT + NDINDIRECT)

// On-disk inode structure
// Contains metadata and block addresses for file data
// Size is fixed at 128 bytes to ensure proper alignment (BSIZE=2048, 2048/128=16)
struct dinode {
	short type;		  // File type (regular, directory, device)
	short major;		  // Major device number (T_DEV only)
	short minor;		  // Minor device number (T_DEV only)
	short nlink;		  // Number of hard links to this inode
	uint size;		  // Size of file content (bytes)
	uint addrs[NDIRECT + 2];  // Block addresses (12 * 4 = 48 bytes)
				  // [0..9]: direct blocks
				  // [10]: single indirect block
				  // [11]: double indirect block
	uint pad[17];		  // Padding: 17 * 4 = 68 bytes
};				  // Total: 8 + 4 + 48 + 68 = 128 bytes ✓

// Number of inodes that fit in one block
#define IPB (BSIZE / sizeof(struct dinode))

// Calculate which block contains inode i
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)

// Number of bitmap bits per block
#define BPB (BSIZE * 8)

// Calculate which block contains the free bit for block b
#define BBLOCK(b, sb) ((b) / BPB + sb.bmapstart)

// Directory entry structure
// Directories are files containing a sequence of these structures
#define DIRSIZ 14

struct dirent {
	ushort inum;	   // Inode number (0 if entry is free)
	char name[DIRSIZ]; // File name (null-terminated if < DIRSIZ)
};

#endif // FS_H